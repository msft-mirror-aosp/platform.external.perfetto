// Copyright (C) 2023 The Android Open Source Project
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

import {Time, time} from '../../base/time';
import {exists} from '../../base/utils';
import {Actions} from '../../common/actions';
import {globals} from '../../frontend/globals';
import {openInOldUIWithSizeCheck} from '../../frontend/legacy_trace_viewer';
import {
  PerfettoPlugin,
  PluginContext,
  PluginContextTrace,
  PluginDescriptor,
} from '../../public';
import {
  isLegacyTrace,
  openFileWithLegacyTraceViewer,
} from '../../frontend/legacy_trace_viewer';
import {DisposableStack} from '../../base/disposable_stack';
import {ADD_SQL_TABLE_TAB_COMMAND_ID} from '../../frontend/sql_table_tab_command';
import {
  addSqlTableTabImpl,
  SqlTableTabConfig,
} from '../../frontend/sql_table_tab';
import {Workspace} from '../../public/workspace';

const SQL_STATS = `
with first as (select started as ts from sqlstats limit 1)
select
    round((max(ended - started, 0))/1e6) as runtime_ms,
    round((started - first.ts)/1e6) as t_start_ms,
    query
from sqlstats, first
order by started desc`;

const ALL_PROCESSES_QUERY = 'select name, pid from process order by name;';

const CPU_TIME_FOR_PROCESSES = `
select
  process.name,
  sum(dur)/1e9 as cpu_sec
from sched
join thread using(utid)
join process using(upid)
group by upid
order by cpu_sec desc
limit 100;`;

const CYCLES_PER_P_STATE_PER_CPU = `
select
  cpu,
  freq,
  dur,
  sum(dur * freq)/1e6 as mcycles
from (
  select
    cpu,
    value as freq,
    lead(ts) over (partition by cpu order by ts) - ts as dur
  from counter
  inner join cpu_counter_track on counter.track_id = cpu_counter_track.id
  where name = 'cpufreq'
) group by cpu, freq
order by mcycles desc limit 32;`;

const CPU_TIME_BY_CPU_BY_PROCESS = `
select
  process.name as process,
  thread.name as thread,
  cpu,
  sum(dur) / 1e9 as cpu_sec
from sched
inner join thread using(utid)
inner join process using(upid)
group by utid, cpu
order by cpu_sec desc
limit 30;`;

const HEAP_GRAPH_BYTES_PER_TYPE = `
select
  o.upid,
  o.graph_sample_ts,
  c.name,
  sum(o.self_size) as total_self_size
from heap_graph_object o join heap_graph_class c on o.type_id = c.id
group by
 o.upid,
 o.graph_sample_ts,
 c.name
order by total_self_size desc
limit 100;`;

class CoreCommandsPlugin implements PerfettoPlugin {
  private readonly disposable = new DisposableStack();

  onActivate(ctx: PluginContext) {
    ctx.registerCommand({
      id: 'perfetto.CoreCommands#ToggleLeftSidebar',
      name: 'Toggle left sidebar',
      callback: () => {
        if (globals.state.sidebarVisible) {
          globals.dispatch(
            Actions.setSidebar({
              visible: false,
            }),
          );
        } else {
          globals.dispatch(
            Actions.setSidebar({
              visible: true,
            }),
          );
        }
      },
      defaultHotkey: '!Mod+B',
    });

    const input = document.createElement('input');
    input.classList.add('trace_file');
    input.setAttribute('type', 'file');
    input.style.display = 'none';
    input.addEventListener('change', onInputElementFileSelectionChanged);
    document.body.appendChild(input);
    this.disposable.defer(() => {
      document.body.removeChild(input);
    });

    const OPEN_TRACE_COMMAND_ID = 'perfetto.CoreCommands#openTrace';
    ctx.registerCommand({
      id: OPEN_TRACE_COMMAND_ID,
      name: 'Open trace file',
      callback: () => {
        delete input.dataset['useCatapultLegacyUi'];
        input.click();
      },
      defaultHotkey: '!Mod+O',
    });
    ctx.addSidebarMenuItem({
      commandId: OPEN_TRACE_COMMAND_ID,
      group: 'navigation',
      icon: 'folder_open',
    });

    const OPEN_LEGACY_TRACE_COMMAND_ID =
      'perfetto.CoreCommands#openTraceInLegacyUi';
    ctx.registerCommand({
      id: OPEN_LEGACY_TRACE_COMMAND_ID,
      name: 'Open with legacy UI',
      callback: () => {
        input.dataset['useCatapultLegacyUi'] = '1';
        input.click();
      },
    });
    ctx.addSidebarMenuItem({
      commandId: OPEN_LEGACY_TRACE_COMMAND_ID,
      group: 'navigation',
      icon: 'filter_none',
    });
  }

  async onTraceLoad(ctx: PluginContextTrace): Promise<void> {
    ctx.registerCommand({
      id: 'perfetto.CoreCommands#RunQueryAllProcesses',
      name: 'Run query: All processes',
      callback: () => {
        ctx.tabs.openQuery(ALL_PROCESSES_QUERY, 'All Processes');
      },
    });

    ctx.registerCommand({
      id: 'perfetto.CoreCommands#RunQueryCpuTimeByProcess',
      name: 'Run query: CPU time by process',
      callback: () => {
        ctx.tabs.openQuery(CPU_TIME_FOR_PROCESSES, 'CPU time by process');
      },
    });

    ctx.registerCommand({
      id: 'perfetto.CoreCommands#RunQueryCyclesByStateByCpu',
      name: 'Run query: cycles by p-state by CPU',
      callback: () => {
        ctx.tabs.openQuery(
          CYCLES_PER_P_STATE_PER_CPU,
          'Cycles by p-state by CPU',
        );
      },
    });

    ctx.registerCommand({
      id: 'perfetto.CoreCommands#RunQueryCyclesByCpuByProcess',
      name: 'Run query: CPU Time by CPU by process',
      callback: () => {
        ctx.tabs.openQuery(
          CPU_TIME_BY_CPU_BY_PROCESS,
          'CPU time by CPU by process',
        );
      },
    });

    ctx.registerCommand({
      id: 'perfetto.CoreCommands#RunQueryHeapGraphBytesPerType',
      name: 'Run query: heap graph bytes per type',
      callback: () => {
        ctx.tabs.openQuery(
          HEAP_GRAPH_BYTES_PER_TYPE,
          'Heap graph bytes per type',
        );
      },
    });

    ctx.registerCommand({
      id: 'perfetto.CoreCommands#DebugSqlPerformance',
      name: 'Debug SQL performance',
      callback: () => {
        ctx.tabs.openQuery(SQL_STATS, 'Recent SQL queries');
      },
    });

    ctx.registerCommand({
      id: 'perfetto.CoreCommands#UnpinAllTracks',
      name: 'Unpin all pinned tracks',
      callback: () => {
        const workspace = ctx.timeline.workspace;
        workspace.pinnedTracks.forEach((t) => workspace.unpinTrack(t));
      },
    });

    ctx.registerCommand({
      id: 'perfetto.CoreCommands#ExpandAllGroups',
      name: 'Expand all track groups',
      callback: () => {
        ctx.timeline.workspace.flatGroups.forEach((g) => g.expand());
      },
    });

    ctx.registerCommand({
      id: 'perfetto.CoreCommands#CollapseAllGroups',
      name: 'Collapse all track groups',
      callback: () => {
        ctx.timeline.workspace.flatGroups.forEach((g) => g.collapse());
      },
    });

    ctx.registerCommand({
      id: 'perfetto.CoreCommands#PanToTimestamp',
      name: 'Pan to timestamp',
      callback: (tsRaw: unknown) => {
        if (exists(tsRaw)) {
          if (typeof tsRaw !== 'bigint') {
            throw Error(`${tsRaw} is not a bigint`);
          }
          ctx.timeline.panToTimestamp(Time.fromRaw(tsRaw));
        } else {
          // No args passed, probably run from the command palette.
          const ts = promptForTimestamp('Enter a timestamp');
          if (exists(ts)) {
            ctx.timeline.panToTimestamp(Time.fromRaw(ts));
          }
        }
      },
    });

    ctx.registerCommand({
      id: 'perfetto.CoreCommands#ShowCurrentSelectionTab',
      name: 'Show current selection tab',
      callback: () => {
        ctx.tabs.showTab('current_selection');
      },
    });

    ctx.registerCommand({
      id: ADD_SQL_TABLE_TAB_COMMAND_ID,
      name: 'Open SQL table viewer',
      callback: (args: unknown) => {
        if (args === undefined) {
          // If we are being run from the command palette, args will be
          // undefined, so there's not a lot we can do here...

          // Perhaps in the future we could just open the table in a new tab and
          // allow the user to browse the tables..?
          return;
        }
        addSqlTableTabImpl(args as SqlTableTabConfig);
      },
    });

    ctx.registerCommand({
      id: 'createNewEmptyWorkspace',
      name: 'Create new empty workspace',
      callback: async () => {
        try {
          const name = await ctx.prompt('Give it a name...');
          const newWorkspace = new Workspace(name);
          globals.workspaces.push(newWorkspace);
          globals.switchWorkspace(newWorkspace);
        } finally {
        }
      },
    });

    ctx.registerCommand({
      id: 'switchWorkspace',
      name: 'Switch workspace',
      callback: async () => {
        try {
          const options = globals.workspaces.map((ws) => {
            return {key: ws.uuid, displayName: ws.displayName};
          });
          const workspaceUuid = await ctx.prompt(
            'Choose a workspace...',
            options,
          );
          const workspace = globals.workspaces.find(
            (ws) => ws.uuid === workspaceUuid,
          );
          if (workspace) {
            globals.switchWorkspace(workspace);
          }
        } finally {
        }
      },
    });
  }

  onDeactivate(_: PluginContext): void {
    this.disposable[Symbol.dispose]();
  }
}

function promptForTimestamp(message: string): time | undefined {
  const tsStr = window.prompt(message);
  if (tsStr !== null) {
    try {
      return Time.fromRaw(BigInt(tsStr));
    } catch {
      window.alert(`${tsStr} is not an integer`);
    }
  }
  return undefined;
}

function onInputElementFileSelectionChanged(e: Event) {
  if (!(e.target instanceof HTMLInputElement)) {
    throw new Error('Not an input element');
  }
  if (!e.target.files) return;
  const file = e.target.files[0];
  // Reset the value so onchange will be fired with the same file.
  e.target.value = '';

  if (e.target.dataset['useCatapultLegacyUi'] === '1') {
    openWithLegacyUi(file);
    return;
  }

  globals.logging.logEvent('Trace Actions', 'Open trace from file');
  globals.dispatch(Actions.openTraceFromFile({file}));
}

async function openWithLegacyUi(file: File) {
  // Switch back to the old catapult UI.
  globals.logging.logEvent('Trace Actions', 'Open trace in Legacy UI');
  if (await isLegacyTrace(file)) {
    openFileWithLegacyTraceViewer(file);
    return;
  }
  openInOldUIWithSizeCheck(file);
}

export const plugin: PluginDescriptor = {
  pluginId: 'perfetto.CoreCommands',
  plugin: CoreCommandsPlugin,
};
