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

import m from 'mithril';

import {CPU_SLICE_TRACK_KIND} from '../../public';
import {SliceDetailsPanel} from '../../frontend/slice_details_panel';
import {
  Engine,
  PerfettoPlugin,
  PluginContextTrace,
  PluginDescriptor,
} from '../../public';
import {NUM, STR_NULL} from '../../trace_processor/query_result';
import {CpuSliceTrack} from './cpu_slice_track';

class CpuSlices implements PerfettoPlugin {
  async onTraceLoad(ctx: PluginContextTrace): Promise<void> {
    const cpus = ctx.trace.cpus;
    const cpuToClusterType = await this.getAndroidCpuClusterTypes(ctx.engine);

    for (const cpu of cpus) {
      const size = cpuToClusterType.get(cpu);
      const uri = `/sched_cpu${cpu}`;

      const name = size === undefined ? `Cpu ${cpu}` : `Cpu ${cpu} (${size})`;
      ctx.registerTrack({
        uri,
        title: name,
        tags: {
          kind: CPU_SLICE_TRACK_KIND,
          cpu,
        },
        trackFactory: ({trackKey}) => {
          return new CpuSliceTrack(ctx.engine, trackKey, cpu);
        },
      });
    }

    ctx.registerDetailsPanel({
      render: (sel) => {
        if (sel.kind === 'SCHED_SLICE') {
          return m(SliceDetailsPanel);
        }
        return undefined;
      },
    });
  }

  async getAndroidCpuClusterTypes(
    engine: Engine,
  ): Promise<Map<number, string>> {
    const cpuToClusterType = new Map<number, string>();
    await engine.query(`
      include perfetto module android.cpu.cluster_type;
    `);
    const result = await engine.query(`
      select cpu, cluster_type as clusterType
      from android_cpu_cluster_mapping
    `);

    const it = result.iter({
      cpu: NUM,
      clusterType: STR_NULL,
    });

    for (; it.valid(); it.next()) {
      const clusterType = it.clusterType;
      if (clusterType !== null) {
        cpuToClusterType.set(it.cpu, clusterType);
      }
    }

    return cpuToClusterType;
  }
}

export const plugin: PluginDescriptor = {
  pluginId: 'perfetto.CpuSlices',
  plugin: CpuSlices,
};
