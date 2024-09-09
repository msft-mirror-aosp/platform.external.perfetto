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
import {Sorting} from '../../../common/state';
import {Area} from '../../../public/selection';
import {globals} from '../../../frontend/globals';
import {Engine} from '../../../trace_processor/engine';
import {CPU_SLICE_TRACK_KIND} from '../../../public/track_kinds';
import {AggregationController} from '../aggregation_controller';
import {hasWattsonSupport} from '../../../core/trace_config_utils';

export class WattsonProcessAggregationController extends AggregationController {
  async createAggregateView(engine: Engine, area: Area) {
    await engine.query(`drop view if exists ${this.kind};`);

    // Short circuit if Wattson is not supported for this Perfetto trace
    if (!(await hasWattsonSupport(engine))) return false;

    const selectedCpus: number[] = [];
    for (const trackUri of area.trackUris) {
      const trackInfo = globals.trackManager.getTrack(trackUri);
      trackInfo?.tags?.kind === CPU_SLICE_TRACK_KIND &&
        exists(trackInfo.tags.cpu) &&
        selectedCpus.push(trackInfo.tags.cpu);
    }
    if (selectedCpus.length === 0) return false;

    const cpusCsv = `(` + selectedCpus.join() + `)`;
    const duration = area.end - area.start;

    // Prerequisite tables are already generated by Wattson thread aggregation,
    // which is run prior to execution of this module
    engine.query(`
      -- Only get idle attribution in user defined window and filter by selected
      -- CPUs and GROUP BY process
      CREATE OR REPLACE PERFETTO TABLE _per_process_idle_attribution AS
      SELECT
        ROUND(SUM(idle_cost_mws), 2) as idle_cost_mws,
        upid
      FROM _filter_idle_attribution(${area.start}, ${duration})
      WHERE cpu in ${cpusCsv}
      GROUP BY upid;

      -- Grouped by UPID and made CPU agnostic
      CREATE VIEW ${this.kind} AS
      SELECT
        ROUND(SUM(total_pws) / ${duration}, 2) as avg_mw,
        ROUND(SUM(total_pws) / 1000000000, 2) as total_mws,
        COALESCE(idle_cost_mws, 0) as idle_cost_mws,
        pid,
        process_name
      FROM _unioned_per_cpu_total
      LEFT JOIN _per_process_idle_attribution USING (upid)
      GROUP BY upid;
    `);

    return true;
  }

  getColumnDefinitions(): ColumnDef[] {
    return [
      {
        title: 'Process Name',
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
        title: 'Average power (estimated mW)',
        kind: 'NUMBER',
        columnConstructor: Float64Array,
        columnId: 'avg_mw',
        sum: true,
      },
      {
        title: 'Total energy (estimated mWs)',
        kind: 'NUMBER',
        columnConstructor: Float64Array,
        columnId: 'total_mws',
        sum: true,
      },
      {
        title: 'Idle transitions overhead (estimated mWs)',
        kind: 'NUMBER',
        columnConstructor: Float64Array,
        columnId: 'idle_cost_mws',
        sum: true,
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
