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
  BaseCounterTrack,
  CounterOptions,
} from '../../frontend/base_counter_track';
import {
  Engine,
  Plugin,
  PluginContextTrace,
  PluginDescriptor,
} from '../../public';

class Wattson implements Plugin {
  async onTraceLoad(ctx: PluginContextTrace): Promise<void> {
    ctx.engine.query(`INCLUDE PERFETTO MODULE wattson.curves.ungrouped;`);

    // CPUs estimate as part of CPU subsystem
    const cpus = globals.traceContext.cpus;
    for (const cpu of cpus) {
      const queryKey = `cpu${cpu}_curve`;
      ctx.registerStaticTrack({
        uri: `perfetto.CpuSubsystemEstimate#CPU${cpu}`,
        displayName: `Cpu${cpu} Estimate`,
        kind: `CpuSubsystemEstimateTrack`,
        trackFactory: ({trackKey}) =>
          new CpuSubsystemEstimateTrack(ctx.engine, trackKey, queryKey),
        groupName: `Wattson`,
      });
    }

    ctx.registerStaticTrack({
      uri: `perfetto.CpuSubsystemEstimate#ScuInterconnect`,
      displayName: `SCU Interconnect Estimate`,
      kind: `CpuSubsystemEstimateTrack`,
      trackFactory: ({trackKey}) =>
        new CpuSubsystemEstimateTrack(ctx.engine, trackKey, `scu`),
      groupName: `Wattson`,
    });
  }
}

class CpuSubsystemEstimateTrack extends BaseCounterTrack {
  readonly engine: Engine;
  readonly queryKey: string;

  constructor(engine: Engine, trackKey: string, queryKey: string) {
    super({
      engine: engine,
      trackKey: trackKey,
    });
    this.engine = engine;
    this.queryKey = queryKey;
  }

  protected getDefaultCounterOptions(): CounterOptions {
    const options = super.getDefaultCounterOptions();
    options.yRangeSharingKey = `CpuSubsystem`;
    options.unit = `mW`;
    return options;
  }

  getSqlSource() {
    if (this.queryKey.startsWith(`cpu`)) {
      return `select ts, ${this.queryKey} as value from _system_state_curves`;
    } else {
      return `
        select
          ts,
          -- L3 values are scaled by 1000 because it's divided by ns and L3 LUTs
          -- are scaled by 10^6. This brings to same units as static_curve (mW)
          ((IFNULL(l3_hit_value, 0) + IFNULL(l3_miss_value, 0)) * 1000 / dur)
            + static_curve  as value
        from _system_state_curves
      `;
    }
  }
}

export const plugin: PluginDescriptor = {
  pluginId: `org.kernel.Wattson`,
  plugin: Wattson,
};
