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

import {sqlTableRegistry} from '../../frontend/widgets/sql/table/sql_table_registry';
import {TrackNode} from '../../public/workspace';
import {Trace} from '../../public/trace';
import {PerfettoPlugin} from '../../public/plugin';
import {ActiveCPUCountTrack, CPUType} from './active_cpu_count';
import {
  RunnableThreadCountTrack,
  UninterruptibleSleepThreadCountTrack,
} from './thread_count';
import {getSchedTable} from './table';
import {extensions} from '../../public/lib/extensions';

export default class implements PerfettoPlugin {
  static readonly id = 'dev.perfetto.Sched';
  async onTraceLoad(ctx: Trace) {
    const runnableThreadCountUri = `/runnable_thread_count`;
    ctx.tracks.registerTrack({
      uri: runnableThreadCountUri,
      title: 'Runnable thread count',
      track: new RunnableThreadCountTrack({
        trace: ctx,
        uri: runnableThreadCountUri,
      }),
    });
    ctx.commands.registerCommand({
      id: 'dev.perfetto.Sched.AddRunnableThreadCountTrackCommand',
      name: 'Add track: runnable thread count',
      callback: () =>
        addPinnedTrack(ctx, runnableThreadCountUri, 'Runnable thread count'),
    });

    const uninterruptibleSleepThreadCountUri = `/uninterruptible_sleep_thread_count`;
    ctx.tracks.registerTrack({
      uri: uninterruptibleSleepThreadCountUri,
      title: 'Uninterruptible Sleep thread count',
      track: new UninterruptibleSleepThreadCountTrack({
        trace: ctx,
        uri: uninterruptibleSleepThreadCountUri,
      }),
    });
    ctx.commands.registerCommand({
      id: 'dev.perfetto.Sched.AddUninterruptibleSleepThreadCountTrackCommand',
      name: 'Add track: uninterruptible sleep thread count',
      callback: () =>
        addPinnedTrack(
          ctx,
          uninterruptibleSleepThreadCountUri,
          'Uninterruptible Sleep thread count',
        ),
    });

    const uri = uriForActiveCPUCountTrack();
    const title = 'Active CPU count';
    ctx.tracks.registerTrack({
      uri,
      title: title,
      track: new ActiveCPUCountTrack({trackUri: uri}, ctx),
    });
    ctx.commands.registerCommand({
      id: 'dev.perfetto.Sched.AddActiveCPUCountTrackCommand',
      name: 'Add track: active CPU count',
      callback: () => addPinnedTrack(ctx, uri, title),
    });

    for (const cpuType of Object.values(CPUType)) {
      const uri = uriForActiveCPUCountTrack(cpuType);
      const title = `Active ${cpuType} CPU count`;
      ctx.tracks.registerTrack({
        uri,
        title: title,
        track: new ActiveCPUCountTrack({trackUri: uri}, ctx, cpuType),
      });

      ctx.commands.registerCommand({
        id: `dev.perfetto.Sched.AddActiveCPUCountTrackCommand.${cpuType}`,
        name: `Add track: active ${cpuType} CPU count`,
        callback: () => addPinnedTrack(ctx, uri, title),
      });
    }

    sqlTableRegistry['sched'] = getSchedTable();
    ctx.commands.registerCommand({
      id: 'perfetto.ShowTable.sched',
      name: 'Open table: sched',
      callback: () => {
        extensions.addSqlTableTab(ctx, {
          table: getSchedTable(),
        });
      },
    });
  }
}

function uriForActiveCPUCountTrack(cpuType?: CPUType): string {
  const prefix = `/active_cpus`;
  if (cpuType !== undefined) {
    return `${prefix}_${cpuType}`;
  } else {
    return prefix;
  }
}

function addPinnedTrack(ctx: Trace, uri: string, title: string) {
  const track = new TrackNode({uri, title});
  // Add track to the top of the stack
  ctx.workspace.addChildFirst(track);
  track.pin();
}
