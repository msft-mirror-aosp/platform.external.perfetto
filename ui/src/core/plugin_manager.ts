// Copyright (C) 2022 The Android Open Source Project
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

import {Registry} from '../base/registry';
import {App} from '../public/app';
import {
  MetricVisualisation,
  PerfettoPlugin,
  PluginDescriptorStatic,
} from '../public/plugin';
import {Trace} from '../public/trace';
import {defaultPlugins} from './default_plugins';
import {featureFlags, Flag} from './feature_flags';
import {TraceImpl} from './trace_impl';

// The pseudo plugin id used for the core instance of AppImpl.
export const CORE_PLUGIN_ID = '__core__';

function makePlugin(
  desc: PluginDescriptorStatic<PerfettoPlugin>,
  trace: Trace,
): PerfettoPlugin {
  const PluginClass = desc;
  return new PluginClass(trace);
}

// This interface injects AppImpl's methods into PluginManager to avoid
// circular dependencies between PluginManager and AppImpl.
export interface PluginAppInterface {
  forkForPlugin(pluginId: string): App;
  get trace(): TraceImpl | undefined;
}

// Contains information about a plugin.
export interface PluginWrapper {
  // A reference to the plugin descriptor
  readonly desc: PluginDescriptorStatic<PerfettoPlugin>;

  // The feature flag used to allow users to change whether this plugin should
  // be enabled or not.
  readonly enableFlag: Flag;

  // Keeps track of whether the plugin has been activated or not.
  active?: boolean;

  // If a trace has been loaded, this object stores the relevant trace-scoped
  // plugin data
  traceContext?: {
    // The concrete plugin instance, created on trace load.
    readonly instance: PerfettoPlugin;

    // How long it took for the plugin's onTraceLoad() function to run.
    readonly loadTimeMs: number;
  };
}

export class PluginManager {
  private readonly registry = new Registry<PluginWrapper>((x) => x.desc.id);

  constructor(private readonly app: PluginAppInterface) {}

  registerPlugin(desc: PluginDescriptorStatic<PerfettoPlugin>) {
    const flagId = `plugin_${desc.id}`;
    const name = `Plugin: ${desc.id}`;
    const flag = featureFlags.register({
      id: flagId,
      name,
      description: `Overrides '${desc.id}' plugin.`,
      defaultValue: defaultPlugins.includes(desc.id),
    });
    this.registry.register({
      desc,
      enableFlag: flag,
    });
  }

  /**
   * Activates all registered plugins that have not already been registered.
   *
   * @param enableOverrides - The list of plugins that are enabled regardless of
   * the current flag setting.
   */
  activatePlugins(enableOverrides: ReadonlyArray<string> = []) {
    this.registry
      .valuesAsArray()
      .filter((p) => p.enableFlag.get() || enableOverrides.includes(p.desc.id))
      .forEach((p) => {
        if (p.active) return;
        const app = this.app.forkForPlugin(p.desc.id);
        p.desc.onActivate?.(app);
        p.active = true;
      });
  }

  async onTraceLoad(
    traceCore: TraceImpl,
    beforeEach?: (id: string) => void,
  ): Promise<void> {
    // Awaiting all plugins in parallel will skew timing data as later plugins
    // will spend most of their time waiting for earlier plugins to load.
    // Running in parallel will have very little performance benefit assuming
    // most plugins use the same engine, which can only process one query at a
    // time.
    for (const p of this.registry.values()) {
      if (p.active) {
        beforeEach?.(p.desc.id);
        const trace = traceCore.forkForPlugin(p.desc.id);
        const before = performance.now();
        const instance = makePlugin(p.desc, trace);
        await instance.onTraceLoad?.(trace);
        const loadTimeMs = performance.now() - before;
        p.traceContext = {
          instance,
          loadTimeMs,
        };
        traceCore.trash.defer(() => {
          p.traceContext = undefined;
        });
      }
    }
  }

  metricVisualisations(): MetricVisualisation[] {
    return this.registry.valuesAsArray().flatMap((plugin) => {
      if (!plugin.active) return [];
      return plugin.desc.metricVisualisations?.() ?? [];
    });
  }

  getAllPlugins() {
    return this.registry.valuesAsArray();
  }

  getPluginContainer(id: string): PluginWrapper | undefined {
    return this.registry.tryGet(id);
  }

  getPlugin(id: string): PerfettoPlugin | undefined {
    return this.registry.tryGet(id)?.traceContext?.instance;
  }
}
