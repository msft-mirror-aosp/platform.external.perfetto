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

import {exists} from '../../../base/utils';
import {ColumnDef} from '../../../common/aggregation_data';
import {Area, Sorting} from '../../../common/state';
import {globals} from '../../../frontend/globals';
import {Engine} from '../../../trace_processor/engine';
import {NUM} from '../../../trace_processor/query_result';
import {CPU_SLICE_TRACK_KIND} from '../../../core/track_kinds';
import {AggregationController} from '../aggregation_controller';

export class WattsonProcessAggregationController extends AggregationController {
  async createAggregateView(engine: Engine, area: Area) {
    await engine.query(`drop view if exists ${this.kind};`);

    // Short circuit if Wattson is not supported for this Perfetto trace
    const deviceInfo = await engine.query(`
        INCLUDE PERFETTO MODULE wattson.device_infos;
        SELECT COUNT(*) as isValid FROM _wattson_device
    `);
    if (deviceInfo.firstRow({isValid: NUM}).isValid === 0) return false;

    const selectedCpus: number[] = [];
    for (const trackKey of area.tracks) {
      const track = globals.state.tracks[trackKey];
      if (track?.uri) {
        const trackInfo = globals.trackManager.resolveTrackInfo(track.uri);
        if (trackInfo?.kind === CPU_SLICE_TRACK_KIND) {
          exists(trackInfo.cpu) && selectedCpus.push(trackInfo.cpu);
        }
      }
    }
    if (selectedCpus.length === 0) return false;

    const duration = area.end - area.start;
    const queryPrefix = `
      DROP TABLE IF EXISTS _ui_selection_window;
      CREATE PERFETTO TABLE _ui_selection_window AS
      SELECT
        ${area.start} as ts,
        ${duration} as dur;
    `;
    engine.query(this.getEstimateProcessQuery(queryPrefix, selectedCpus));

    return true;
  }

  // This function returns a query that gets the average and estimate from
  // Wattson for the selection in the UI window based on process. The grouping
  // by thread needs to 'remove' 2 dimensions; the threads need to be grouped
  // over time and the processes need to be grouped over CPUs.
  // 1. Window and associate process with proper Wattson estimate slice
  // 2. Group all processes over time on a per CPU basis
  // 3. Group all processes over all CPUs
  getEstimateProcessQuery(queryPrefix: string, selectedCpus: number[]): string {
    let query = queryPrefix;

    // Estimate and total per UPID per CPU
    selectedCpus.forEach((cpu) => {
      query += `
        -- Processes filtered by CPU
        DROP TABLE IF EXISTS _per_cpu_threads;
        CREATE PERFETTO TABLE _per_cpu_threads AS
        SELECT ts, dur, cpu, utid
        FROM sched WHERE cpu = ${cpu};

        -- Processes filtered by CPU within the UI defined time window
        DROP TABLE IF EXISTS _windowed_per_cpu_threads;
        CREATE VIRTUAL TABLE _windowed_per_cpu_threads
        USING
          SPAN_JOIN(_ui_selection_window, _per_cpu_threads);

        -- CPU specific track with slices for curves
        DROP TABLE IF EXISTS _per_cpu_curve;
        CREATE PERFETTO TABLE _per_cpu_curve AS
        SELECT ts, dur, cpu${cpu}_curve
        FROM _system_state_curves;

        -- Filter out track when threads are available
        DROP TABLE IF EXISTS _windowed_thread_curve;
        CREATE VIRTUAL TABLE _windowed_thread_curve
        USING SPAN_JOIN(_per_cpu_curve, _windowed_per_cpu_threads);

        -- Total estimate per UPID per CPU
        DROP TABLE IF EXISTS _total_per_process_cpu${cpu};
        CREATE PERFETTO TABLE _total_per_process_cpu${cpu} AS
        SELECT
          SUM(cpu${cpu}_curve * dur) as total_pws,
          SUM(dur) as dur,
          COUNT(dur) as occurences,
          process.upid,
          process.name as p_name,
          thread.name as t_name,
          cpu
        FROM _windowed_thread_curve as _thread_lvl
        JOIN thread on _thread_lvl.utid = thread.utid
        JOIN process on thread.upid = process.upid
        GROUP BY thread.upid;
      `;
    });

    // Estimate and total per UPID, removing CPU dimension
    query += `
      -- Grouped again by UPID, but this time to make it CPU agnostic
      CREATE VIEW ${this.kind} AS
      WITH _unioned_per_process_per_cpu AS (
    `;
    selectedCpus.forEach((cpu, i) => {
      query += i != 0 ? `UNION ALL\n` : ``;
      query += `SELECT * from _total_per_process_cpu${cpu}\n`;
    });
    query += `
      )
      SELECT
        ROUND(SUM(total_pws) / SUM(dur), 2) as avg_mw,
        ROUND(SUM(total_pws) / 1000000000, 2) as total_mws,
        ROUND(SUM(dur) / 1000000.0, 2) as dur,
        SUM(occurences) as occurences,
        upid,
        p_name,
        t_name
      FROM _unioned_per_process_per_cpu
      GROUP BY upid;
    `;

    return query;
  }

  getColumnDefinitions(): ColumnDef[] {
    return [
      {
        title: 'Process Name',
        kind: 'STRING',
        columnConstructor: Uint16Array,
        columnId: 'p_name',
      },
      {
        title: 'UPID',
        kind: 'NUMBER',
        columnConstructor: Uint16Array,
        columnId: 'upid',
      },
      {
        title: 'Occurences',
        kind: 'NUMBER',
        columnConstructor: Uint16Array,
        columnId: 'occurences',
      },
      {
        title: 'Total Duration (ms)',
        kind: 'NUMBER',
        columnConstructor: Float64Array,
        columnId: 'dur',
      },
      {
        title: 'Average estimate (mW)',
        kind: 'NUMBER',
        columnConstructor: Float64Array,
        columnId: 'avg_mw',
      },
      {
        title: 'Total estimate (mWs)',
        kind: 'NUMBER',
        columnConstructor: Float64Array,
        columnId: 'total_mws',
      },
    ];
  }

  async getExtra() {}

  getTabName() {
    return 'Wattson by process';
  }

  getDefaultSorting(): Sorting {
    return {column: 'total_mws', direction: 'DESC'};
  }
}
