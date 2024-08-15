
-- Copyright 2024 The Android Open Source Project
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     https://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.

INCLUDE PERFETTO MODULE wattson.curves.ungrouped;

-- The power calculations need to use the same time period in which energy
-- calculations were made for consistency
DROP VIEW IF EXISTS _wattson_period_windows;
CREATE PERFETTO VIEW _wattson_period_windows AS
SELECT
  MIN(ts) as ts,
  MAX(ts) - MIN(ts) as dur,
  1 as period_id
FROM _system_state_mw;

SELECT RUN_METRIC(
  'android/wattson_rail_relations.sql',
  'window_table', '_wattson_period_windows'
);

DROP VIEW IF EXISTS wattson_trace_rails_output;
CREATE PERFETTO VIEW wattson_trace_rails_output AS
SELECT AndroidWattsonTimePeriodMetric(
  'metric_version', 2,
  'period_info', (
    SELECT RepeatedField(
      AndroidWattsonEstimateInfo(
        'period_id', period_id,
        'period_dur', period_dur,
        'cpu_subsystem', proto
      )
    )
    FROM _estimate_cpu_subsystem_sum
  )
);
