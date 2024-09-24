// Copyright (C) 2024 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import {DisposableStack} from '../base/disposable_stack';
import {assertTrue} from '../base/logging';
import {createStore, Migrate, Store} from '../base/store';
import {TimeSpan} from '../base/time';
import {TimelineImpl} from '../core/timeline';
import {App} from '../public/app';
import {Command} from '../public/command';
import {DetailsPanel, LegacyDetailsPanel} from '../public/details_panel';
import {Trace} from '../public/trace';
import {setScrollToFunction} from '../public/scroll_helper';
import {ScrollToArgs} from 'src/public/scroll_helper';
import {TraceInfo} from '../public/trace_info';
import {TrackDescriptor} from '../public/track';
import {EngineBase, EngineProxy} from '../trace_processor/engine';
import {CommandManagerImpl} from './command_manager';
import {NoteManagerImpl} from './note_manager';
import {OmniboxManagerImpl} from './omnibox_manager';
import {SearchManagerImpl} from './search_manager';
import {SelectionManagerImpl} from './selection_manager';
import {SidebarManagerImpl} from './sidebar_manager';
import {TabManagerImpl} from './tab_manager';
import {TrackManagerImpl} from './track_manager';
import {WorkspaceManagerImpl} from './workspace_manager';
import {SidebarMenuItem} from '../public/sidebar';
import {ScrollHelper} from './scroll_helper';
import {Selection, SelectionOpts} from '../public/selection';
import {SearchResult} from '../public/search';

// The pseudo plugin id used for the core instance of AppImpl.
export const CORE_PLUGIN_ID = '__core__';

/**
 * Handles the global state of the ui, for anything that is not related to a
 * specific trace. This is always available even before a trace is loaded (in
 * contrast to TraceContext, which is bound to the lifetime of a trace).
 * There is only one instance in total of this class (see instance()).
 * This class is not exposed to anybody. Both core and plugins should access
 * this via AppImpl.
 */
class AppContext {
  readonly commandMgr = new CommandManagerImpl();
  readonly omniboxMgr = new OmniboxManagerImpl();
  readonly sidebarMgr = new SidebarManagerImpl();

  // The most recently created trace context. Can be undefined before any trace
  // is loaded.
  private traceCtx?: TraceContext;

  // There is only one global instance, lazily initialized on the first call.
  private static _instance: AppContext;
  static get instance() {
    return (AppContext._instance = AppContext._instance ?? new AppContext());
  }

  private constructor() {}

  get currentTraceCtx(): TraceContext | undefined {
    return this.traceCtx;
  }

  // Called by AppImpl.newTraceInstance().
  setActiveTrace(traceCtx: TraceContext | undefined) {
    if (this.traceCtx !== undefined) {
      // This will trigger the unregistration of trace-scoped commands and
      // sidebar menuitems (and few similar things).
      this.traceCtx[Symbol.dispose]();
    }
    this.traceCtx = traceCtx;

    // TODO(primiano): remove this injection once we plumb Trace everywhere.
    setScrollToFunction((args: ScrollToArgs) =>
      traceCtx?.scrollHelper.scrollTo(args),
    );
  }
}

/*
 * Every plugin gets its own instance. This is how we keep track
 * what each plugin is doing and how we can blame issues on particular
 * plugins.
 * The instance exists for the whole duration a plugin is active.
 */
export class AppImpl implements App {
  private appCtx: AppContext;
  readonly pluginId: string;
  private currentTrace?: TraceImpl;

  private constructor(appCtx: AppContext, pluginId: string) {
    this.appCtx = appCtx;
    this.pluginId = pluginId;
  }

  // Gets access to the one instance that the core can use. Note that this is
  // NOT the only instance, as other AppImpl instance will be created for each
  // plugin.
  private static _instance: AppImpl;
  static get instance(): AppImpl {
    AppImpl._instance =
      AppImpl._instance ?? new AppImpl(AppContext.instance, CORE_PLUGIN_ID);
    return AppImpl._instance;
  }

  get commands(): CommandManagerImpl {
    return this.appCtx.commandMgr;
  }

  get sidebar(): SidebarManagerImpl {
    return this.appCtx.sidebarMgr;
  }

  get omnibox(): OmniboxManagerImpl {
    return this.appCtx.omniboxMgr;
  }

  get trace(): TraceImpl | undefined {
    return this.currentTrace;
  }

  closeCurrentTrace() {
    this.currentTrace = undefined;
    this.appCtx.setActiveTrace(undefined);
  }

  // This is called by TraceController when loading a new trace, soon after the
  // engine has been set up. It obtains a new TraceImpl for the core. From that
  // we can fork sibling instances (i.e. bound to the same TraceContext) for
  // the various plugins.
  newTraceInstance(engine: EngineBase, traceInfo: TraceInfo): TraceImpl {
    const traceCtx = new TraceContext(this.appCtx, engine, traceInfo);
    this.appCtx.setActiveTrace(traceCtx);
    const newTrace = new TraceImpl(this, traceCtx);
    this.currentTrace = newTrace;
    return this.currentTrace;
  }

  forkForPlugin(pluginId: string): AppImpl {
    assertTrue(pluginId != CORE_PLUGIN_ID);
    return new AppImpl(this.appCtx, pluginId);
  }
}

/**
 * Handles the per-trace state of the UI
 * There is an instance of this class per each trace loaded, and typically
 * between 0 and 1 instances in total (% brief moments while we swap traces).
 * 90% of the app state live here, including the Engine.
 * This is the underlying storage for AppImpl, which instead has one instance
 * per trace per plugin.
 */
class TraceContext implements Disposable {
  readonly appCtx: AppContext;
  readonly engine: EngineBase;
  readonly omniboxMgr = new OmniboxManagerImpl();
  readonly searchMgr: SearchManagerImpl;
  readonly selectionMgr: SelectionManagerImpl;
  readonly tabMgr = new TabManagerImpl();
  readonly timeline: TimelineImpl;
  readonly traceInfo: TraceInfo;
  readonly trackMgr = new TrackManagerImpl();
  readonly workspaceMgr = new WorkspaceManagerImpl();
  readonly noteMgr = new NoteManagerImpl();
  readonly pluginSerializableState = createStore<{[key: string]: {}}>({});
  readonly scrollHelper: ScrollHelper;
  readonly trash = new DisposableStack();

  constructor(gctx: AppContext, engine: EngineBase, traceInfo: TraceInfo) {
    this.appCtx = gctx;
    this.engine = engine;
    this.traceInfo = traceInfo;
    const traceSpan = new TimeSpan(traceInfo.start, traceInfo.end);
    this.timeline = new TimelineImpl(traceSpan);

    this.scrollHelper = new ScrollHelper(
      this.traceInfo,
      this.timeline,
      this.workspaceMgr.currentWorkspace,
      this.trackMgr,
    );

    this.selectionMgr = new SelectionManagerImpl(
      this.engine,
      this.trackMgr,
      this.noteMgr,
      this.scrollHelper,
      this.onSelectionChange.bind(this),
    );

    this.searchMgr = new SearchManagerImpl({
      timeline: this.timeline,
      trackManager: this.trackMgr,
      engine: this.engine,
      workspace: this.workspaceMgr.currentWorkspace,
      onResultStep: this.onResultStep.bind(this),
    });
  }

  // This method wires up changes to selection to side effects on search and
  // tabs. This is to avoid entangling too many dependencies between managers.
  private onSelectionChange(_: Selection, opts: SelectionOpts) {
    const {clearSearch = true, switchToCurrentSelectionTab = true} = opts;
    if (clearSearch) {
      this.searchMgr.reset();
    }
    if (switchToCurrentSelectionTab) {
      this.tabMgr.showCurrentSelectionTab();
    }
    // pendingScrollId is handled by SelectionManager internally.

    // TODO(primiano): this is temporarily necessary until we kill
    // controllers. The flow controller needs to be re-kicked when we change
    // the selection.
    rerunControllersFunction?.();
  }

  private onResultStep(searchResult: SearchResult) {
    this.selectionMgr.selectSearchResult(searchResult);
  }

  [Symbol.dispose]() {
    this.trash.dispose();
  }
}

/**
 * This implementation provides the plugin access to trace related resources,
 * such as the engine and the store. This exists for the whole duration a plugin
 * is active AND a trace is loaded.
 * There are N+1 instances of this for each trace, one for each plugin plus one
 * for the core.
 */
export class TraceImpl implements Trace {
  private appImpl: AppImpl;
  private traceCtx: TraceContext;

  // This is not the original Engine base, rather an EngineProxy based on the
  // same engineBase.
  private engineProxy: EngineProxy;
  private trackMgrProxy: TrackManagerImpl;
  private commandMgrProxy: CommandManagerImpl;
  private sidebarProxy: SidebarManagerImpl;

  constructor(appImpl: AppImpl, ctx: TraceContext) {
    const pluginId = appImpl.pluginId;
    this.appImpl = appImpl;
    this.traceCtx = ctx;
    const traceUnloadTrash = ctx.trash;

    // Invalidate all the engine proxies when the TraceContext is destroyed.
    this.engineProxy = ctx.engine.getProxy(pluginId);
    traceUnloadTrash.use(this.engineProxy);

    // Intercept the registerTrack() method to inject the pluginId into tracks.
    this.trackMgrProxy = createProxy(ctx.trackMgr, {
      registerTrack(trackDesc: TrackDescriptor): Disposable {
        return ctx.trackMgr.registerTrack({...trackDesc, pluginId});
      },
    });

    // CommandManager is global. Here we intercept the registerCommand() because
    // we want any commands registered via the Trace interface to be
    // unregistered when the trace unloads (before a new trace is loaded) to
    // avoid ending up with duplicate commands.
    this.commandMgrProxy = createProxy(ctx.appCtx.commandMgr, {
      registerCommand(cmd: Command): Disposable {
        const disposable = appImpl.commands.registerCommand(cmd);
        traceUnloadTrash.use(disposable);
        return disposable;
      },
    });

    // Likewise, remove all trace-scoped sidebar entries when the trace unloads.
    this.sidebarProxy = createProxy(ctx.appCtx.sidebarMgr, {
      addMenuItem(menuItem: SidebarMenuItem): Disposable {
        const disposable = appImpl.sidebar.addMenuItem(menuItem);
        traceUnloadTrash.use(disposable);
        return disposable;
      },
    });
  }

  scrollTo(where: ScrollToArgs): void {
    this.traceCtx.scrollHelper.scrollTo(where);
  }

  // Creates an instance of TraceImpl backed by the same TraceContext for
  // another plugin. This is effectively a way to "fork" the core instance and
  // create the N instances for plugins.
  forkForPlugin(pluginId: string) {
    assertTrue(pluginId != CORE_PLUGIN_ID);
    return new TraceImpl(this.appImpl.forkForPlugin(pluginId), this.traceCtx);
  }

  registerDetailsPanel(
    detailsPanel: DetailsPanel | LegacyDetailsPanel,
  ): Disposable {
    if (detailsPanel.panelType === 'LegacyDetailsPanel') {
      return this.traceCtx.tabMgr.registerLegacyDetailsPanel(detailsPanel);
    } else {
      return this.traceCtx.tabMgr.registerDetailsPanel(detailsPanel);
    }
  }

  // TODO(primiano): there are two things here:
  // 1. I'm not sure this belongs to here (see comment in public/trace.ts).
  // 2. Even if we agree it belongs here, right now the dependencies are too
  //    enagnled and this needs to be injected by globals. This can be supported
  //    once we clean up properly queryResult and debug tracks, so that they
  //    don't depend on globals and take a Trace as argument.
  static addQueryResultsTabFunction?: (query: string, title: string) => void;
  addQueryResultsTab(query: string, title: string) {
    return TraceImpl.addQueryResultsTabFunction?.(query, title);
  }

  mountStore<T>(migrate: Migrate<T>): Store<T> {
    return this.traceCtx.pluginSerializableState.createSubStore(
      [this.pluginId],
      migrate,
    );
  }

  getPluginStoreForSerialization() {
    return this.traceCtx.pluginSerializableState;
  }

  get openerPluginArgs(): {[key: string]: unknown} | undefined {
    const traceSource = this.traceCtx.traceInfo.source;
    if (traceSource.type !== 'ARRAY_BUFFER') {
      return undefined;
    }
    const pluginArgs = traceSource.pluginArgs;
    return (pluginArgs ?? {})[this.pluginId];
  }

  get engine() {
    return this.engineProxy;
  }

  get timeline() {
    return this.traceCtx.timeline;
  }

  get tracks() {
    return this.trackMgrProxy;
  }

  get tabs() {
    return this.traceCtx.tabMgr;
  }

  get workspace() {
    return this.traceCtx.workspaceMgr.currentWorkspace;
  }

  get workspaces() {
    return this.traceCtx.workspaceMgr;
  }

  get search() {
    return this.traceCtx.searchMgr;
  }

  get selection() {
    return this.traceCtx.selectionMgr;
  }

  get traceInfo(): TraceInfo {
    return this.traceCtx.traceInfo;
  }

  get notes() {
    return this.traceCtx.noteMgr;
  }

  // App interface implementation.

  get pluginId(): string {
    return this.appImpl.pluginId;
  }

  get commands(): CommandManagerImpl {
    return this.commandMgrProxy;
  }

  get sidebar(): SidebarManagerImpl {
    return this.sidebarProxy;
  }

  get omnibox(): OmniboxManagerImpl {
    return this.appImpl.omnibox;
  }
}

// Allows to take an existing class instance (`target`) and override some of its
// methods via `overrides`. We use this for cases where we want to expose a
// "manager" (e.g. TrackManager, SidebarManager) to the plugins, but we want to
// override few of its methods (e.g. to inject the pluginId in the args).
function createProxy<T extends object>(target: T, overrides: Partial<T>): T {
  return new Proxy(target, {
    get: (target: T, prop: string | symbol, receiver) => {
      // If the property is overriden, use that; otherwise, use target
      const overrideValue = (overrides as {[key: symbol | string]: {}})[prop];
      if (overrideValue !== undefined) {
        return typeof overrideValue === 'function'
          ? overrideValue.bind(overrides)
          : overrideValue;
      }
      const baseValue = Reflect.get(target, prop, receiver);
      return typeof baseValue === 'function'
        ? baseValue.bind(target)
        : baseValue;
    },
  }) as T;
}

// TODO(primiano): remove this once we get rid of controllers. This function
// gets bound to `globals.dispatch(Actions.runControllers({}));` and exists
// only to avoid a circular dependency between globals.ts and this file.
let rerunControllersFunction: () => void;
export function setRerunControllersFunction(f: () => void) {
  rerunControllersFunction = f;
}
