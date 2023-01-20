#!/usr/bin/env python3
# Copyright (C) 2023 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License a
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from python.generators.diff_tests.testing import Path, Metric
from python.generators.diff_tests.testing import Csv, Json, TextProto
from python.generators.diff_tests.testing import DiffTestBlueprint
from python.generators.diff_tests.testing import DiffTestModule


class DiffTestModule_Android(DiffTestModule):

  def test_game_intervention_list_test(self):
    return DiffTestBlueprint(
        trace=Path('game_intervention_list_test.textproto'),
        query="""
SELECT
  package_name,
  uid,
  current_mode,
  standard_mode_supported,
  standard_mode_downscale,
  standard_mode_use_angle,
  standard_mode_fps,
  perf_mode_supported,
  perf_mode_downscale,
  perf_mode_use_angle,
  perf_mode_fps,
  battery_mode_supported,
  battery_mode_downscale,
  battery_mode_use_angle,
  battery_mode_fps
FROM android_game_intervention_list
ORDER BY package_name;
""",
        out=Path('game_intervention_list_test.out'))

  def test_android_system_property_counter(self):
    return DiffTestBlueprint(
        trace=Path('android_system_property.textproto'),
        query="""
SELECT t.id, t.type, t.name, c.id, c.ts, c.type, c.value
FROM counter_track t JOIN counter c ON t.id = c.track_id
WHERE name = 'ScreenState';
""",
        out=Csv("""
"id","type","name","id","ts","type","value"
0,"counter_track","ScreenState",0,1000,"counter",2.000000
0,"counter_track","ScreenState",1,2000,"counter",1.000000
"""))

  def test_android_system_property_slice(self):
    return DiffTestBlueprint(
        trace=Path('android_system_property.textproto'),
        query="""
SELECT t.id, t.type, t.name, s.id, s.ts, s.dur, s.type, s.name
FROM track t JOIN slice s ON s.track_id = t.id
WHERE t.name = 'DeviceStateChanged';
""",
        out=Path('android_system_property_slice.out'))

  def test_android_bugreport_logs_test(self):
    return DiffTestBlueprint(
        trace=Path('../../data/bugreport-crosshatch-SPB5.zip'),
        query="""
WITH
initial AS (SELECT
    (SELECT count(*) FROM android_logs) AS cnt,
    ts, prio, tag, msg FROM android_logs
  ORDER BY ts ASC
  LIMIT 100
),
latest AS (SELECT
    (SELECT count(*) FROM android_logs) AS cnt,
    ts, prio, tag, msg FROM android_logs
  ORDER BY ts DESC
  LIMIT 100
)
SELECT * FROM initial UNION ALL SELECT * FROM latest;
""",
        out=Path('android_bugreport_logs_test.out'))

  def test_android_bugreport_dumpstate_test(self):
    return DiffTestBlueprint(
        trace=Path('../../data/bugreport-crosshatch-SPB5.zip'),
        query="""
SELECT section, service, count(line) AS linecount FROM android_dumpstate
GROUP BY section, service;
""",
        out=Path('android_bugreport_dumpstate_test.out'))

  def test_android_bugreport_dumpsys_test(self):
    return DiffTestBlueprint(
        trace=Path('../../data/bugreport-crosshatch-SPB5.zip'),
        query="""
SELECT section, service, line FROM android_dumpstate
WHERE service = 'color_display';
""",
        out=Path('android_bugreport_dumpsys_test.out'))
