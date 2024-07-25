// Copyright (C) 2021 The Android Open Source Project
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

import {exists} from '../../base/utils';
import {ColumnDef} from '../../common/aggregation_data';
import {Area, Sorting} from '../../common/state';
import {globals} from '../../frontend/globals';
import {Engine} from '../../trace_processor/engine';
import {CPU_SLICE_TRACK_KIND} from '../../core/track_kinds';

import {AggregationController} from './aggregation_controller';

export class CpuByProcessAggregationController extends AggregationController {
  async createAggregateView(engine: Engine, area: Area) {
    await engine.query(`drop view if exists ${this.kind};`);

    const selectedCpus: number[] = [];
    for (const trackKey of area.tracks) {
      const track = globals.state.tracks[trackKey];
      if (track?.uri) {
        const trackInfo = globals.trackManager.resolveTrackInfo(track.uri);
        if (trackInfo?.tags?.kind === CPU_SLICE_TRACK_KIND) {
          exists(trackInfo.tags.cpu) && selectedCpus.push(trackInfo.tags.cpu);
        }
      }
    }
    if (selectedCpus.length === 0) return false;

    const query = `
        INCLUDE PERFETTO MODULE viz.summary.threads_w_processes;

        create view ${this.kind} as
        SELECT
          process_name,
          pid,
          sum(dur) AS total_dur,
          sum(dur)/count(1) as avg_dur,
          count(1) as occurrences
        FROM _state_w_thread_process_summary
        WHERE
          cpu IN (${selectedCpus})
          AND upid is NOT NULL
          AND state = "Running"
          AND ts + dur > ${area.start}
          AND ts < ${area.end}
        GROUP by upid`;

    await engine.query(query);
    return true;
  }

  getTabName() {
    return 'CPU by process';
  }

  async getExtra() {}

  getDefaultSorting(): Sorting {
    return {column: 'total_dur', direction: 'DESC'};
  }

  getColumnDefinitions(): ColumnDef[] {
    return [
      {
        title: 'Process',
        kind: 'STRING',
        columnConstructor: Uint16Array,
        columnId: 'process_name',
      },
      {
        title: 'PID',
        kind: 'NUMBER',
        columnConstructor: Uint16Array,
        columnId: 'pid',
      },
      {
        title: 'Wall duration (ms)',
        kind: 'TIMESTAMP_NS',
        columnConstructor: Float64Array,
        columnId: 'total_dur',
        sum: true,
      },
      {
        title: 'Avg Wall duration (ms)',
        kind: 'TIMESTAMP_NS',
        columnConstructor: Float64Array,
        columnId: 'avg_dur',
      },
      {
        title: 'Occurrences',
        kind: 'NUMBER',
        columnConstructor: Uint16Array,
        columnId: 'occurrences',
        sum: true,
      },
    ];
  }
}
