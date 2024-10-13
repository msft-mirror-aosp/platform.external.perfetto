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

import {globals} from '../../frontend/globals';
import {
  SimpleCounterTrack,
  SimpleCounterTrackConfig,
} from '../../frontend/simple_counter_track';
import {PerfettoPlugin, PluginDescriptor} from '../../public/plugin';
import {
  getOrCreateGroupForProcess,
  getOrCreateGroupForThread,
} from '../../public/standard_groups';
import {Trace} from '../../public/trace';
import {TrackNode} from '../../public/workspace';
import {NUM_NULL} from '../../trace_processor/query_result';

function registerAllocsTrack(
  ctx: Trace,
  uri: string,
  config: SimpleCounterTrackConfig,
): void {
  ctx.tracks.registerTrack({
    uri,
    title: `dmabuf allocs`,
    track: new SimpleCounterTrack(ctx, {trackUri: uri}, config),
  });
}

class AndroidDmabuf implements PerfettoPlugin {
  async onTraceLoad(ctx: Trace): Promise<void> {
    const e = ctx.engine;
    await e.query(`INCLUDE PERFETTO MODULE android.memory.dmabuf`);
    await e.query(`
      CREATE PERFETTO TABLE _android_memory_cumulative_dmabuf AS
      SELECT
        upid, utid, ts,
        SUM(buf_size) OVER(PARTITION BY COALESCE(upid, utid) ORDER BY ts) AS value
      FROM android_dmabuf_allocs;`);

    const pids = await e.query(
      `SELECT DISTINCT upid, IIF(upid IS NULL, utid, NULL) AS utid FROM _android_memory_cumulative_dmabuf`,
    );
    const it = pids.iter({upid: NUM_NULL, utid: NUM_NULL});
    for (; it.valid(); it.next()) {
      if (it.upid != null) {
        const uri = `/android_process_dmabuf_upid_${it.upid}`;
        const config: SimpleCounterTrackConfig = {
          data: {
            sqlSource: `SELECT ts, value FROM _android_memory_cumulative_dmabuf
                 WHERE upid = ${it.upid}`,
            columns: ['ts', 'value'],
          },
          columns: {ts: 'ts', value: 'value'},
        };
        registerAllocsTrack(ctx, uri, config);
        getOrCreateGroupForProcess(globals.workspace, it.upid).addChildInOrder(
          new TrackNode({uri, title: 'dmabuf allocs'}),
        );
      } else if (it.utid != null) {
        const uri = `/android_process_dmabuf_utid_${it.utid}`;
        const config: SimpleCounterTrackConfig = {
          data: {
            sqlSource: `SELECT ts, value FROM _android_memory_cumulative_dmabuf
                 WHERE utid = ${it.utid}`,
            columns: ['ts', 'value'],
          },
          columns: {ts: 'ts', value: 'value'},
        };
        registerAllocsTrack(ctx, uri, config);
        getOrCreateGroupForThread(globals.workspace, it.utid).addChildInOrder(
          new TrackNode({uri, title: 'dmabuf allocs'}),
        );
      }
    }
  }
}

export const plugin: PluginDescriptor = {
  pluginId: 'dev.perfetto.AndroidDmabuf',
  plugin: AndroidDmabuf,
};
