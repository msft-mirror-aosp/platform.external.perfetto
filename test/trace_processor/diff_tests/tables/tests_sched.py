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

from python.generators.diff_tests.testing import Path, DataPath, Metric
from python.generators.diff_tests.testing import Csv, Json, TextProto, BinaryProto
from python.generators.diff_tests.testing import DiffTestBlueprint
from python.generators.diff_tests.testing import TestSuite
from python.generators.diff_tests.testing import PrintProfileProto


class TablesSched(TestSuite):
  # Sched table
  def test_synth_1_filter_sched(self):
    return DiffTestBlueprint(
        trace=Path('../common/synth_1.py'),
        query="""
        SELECT ts, cpu, dur FROM sched
        WHERE
          cpu = 1
          AND dur > 50
          AND dur <= 100
          AND ts >= 100
          AND ts <= 400;
        """,
        out=Csv("""
        "ts","cpu","dur"
        170,1,80
        """))

  def test_android_sched_and_ps_b119496959(self):
    return DiffTestBlueprint(
        trace=DataPath('android_sched_and_ps.pb'),
        query="""
        SELECT ts, cpu FROM sched WHERE ts >= 81473797418963 LIMIT 10;
        """,
        out=Csv("""
        "ts","cpu"
        81473797824982,3
        81473797942847,3
        81473798135399,0
        81473798786857,2
        81473798875451,3
        81473799019930,2
        81473799079982,0
        81473800089357,3
        81473800144461,3
        81473800441805,3
        """))

  def test_android_sched_and_ps_b119301023(self):
    return DiffTestBlueprint(
        trace=DataPath('android_sched_and_ps.pb'),
        query="""
        SELECT ts FROM sched
        WHERE ts > 0.1 + 1e9
        LIMIT 10;
        """,
        out=Csv("""
        "ts"
        81473010031230
        81473010109251
        81473010121751
        81473010179772
        81473010203886
        81473010234720
        81473010278522
        81473010308470
        81473010341386
        81473010352792
        """))

  def test_sched_wakeup(self):
    return DiffTestBlueprint(
        trace=DataPath('sched_wakeup_trace.atr'),
        query="""
        SELECT * FROM spurious_sched_wakeup
        ORDER BY ts LIMIT 10
        """,
        out=Csv("""
        "id","type","ts","thread_state_id","irq_context","utid","waker_utid"
        0,"spurious_sched_wakeup",1735850782904,423,0,230,1465
        1,"spurious_sched_wakeup",1736413914899,886,0,230,1467
        2,"spurious_sched_wakeup",1736977755745,1298,0,230,1469
        3,"spurious_sched_wakeup",1737046900004,1473,0,1472,1473
        4,"spurious_sched_wakeup",1737047159060,1502,0,1474,1472
        5,"spurious_sched_wakeup",1737081636170,2992,0,1214,1319
        6,"spurious_sched_wakeup",1737108696536,5010,0,501,557
        7,"spurious_sched_wakeup",1737153309978,6431,0,11,506
        8,"spurious_sched_wakeup",1737165240546,6915,0,565,499
        9,"spurious_sched_wakeup",1737211563344,8999,0,178,1195
        """))

  def test_sched_waker_id(self):
    return DiffTestBlueprint(
        trace=DataPath('sched_wakeup_trace.atr'),
        query="""
        SELECT parent.id
        FROM thread_state parent
        JOIN thread_state child
          ON parent.utid = child.waker_utid AND child.ts BETWEEN parent.ts AND parent.ts + parent.dur
        WHERE child.id = 15750
        UNION ALL
        SELECT waker_id AS id FROM thread_state WHERE id = 15750
        """,
        out=Csv("""
        "id"
        15748
        15748
        """))

  def test_raw_common_flags(self):
    return DiffTestBlueprint(
        trace=DataPath('sched_wakeup_trace.atr'),
        query="""
        SELECT id, type, ts, name, cpu, utid, arg_set_id, common_flags
        FROM raw WHERE common_flags != 0 ORDER BY ts LIMIT 10
        """,
        out=Csv("""
        "id","type","ts","name","cpu","utid","arg_set_id","common_flags"
        3,"ftrace_event",1735489788930,"sched_waking",0,300,4,1
        4,"ftrace_event",1735489812571,"sched_waking",0,300,5,1
        5,"ftrace_event",1735489833977,"sched_waking",1,305,6,1
        8,"ftrace_event",1735489876788,"sched_waking",1,297,9,1
        9,"ftrace_event",1735489879097,"sched_waking",0,304,10,1
        12,"ftrace_event",1735489933912,"sched_waking",0,428,13,1
        14,"ftrace_event",1735489972385,"sched_waking",1,232,15,1
        17,"ftrace_event",1735489999987,"sched_waking",1,232,15,1
        19,"ftrace_event",1735490039439,"sched_waking",1,298,18,1
        20,"ftrace_event",1735490042084,"sched_waking",1,298,19,1
        """))

  def test_thread_executing_span_graph(self):
    return DiffTestBlueprint(
        trace=DataPath('sched_wakeup_trace.atr'),
        query="""
        INCLUDE PERFETTO MODULE sched.thread_executing_span;
        SELECT
          waker_id,
          prev_id,
          prev_end_ts,
          id,
          ts,
          end_ts,
          is_kernel,
          utid,
          state,
          blocked_function
        FROM _wakeup_graph
        ORDER BY ts
        LIMIT 10
        """,
        out=Csv("""
        "waker_id","prev_id","prev_end_ts","id","ts","end_ts","is_kernel","utid","state","blocked_function"
        "[NULL]","[NULL]","[NULL]",5,1735489812571,1735489896509,0,304,"[NULL]","[NULL]"
        6,"[NULL]","[NULL]",11,1735489876788,1735489953773,0,428,"[NULL]","[NULL]"
        5,"[NULL]","[NULL]",12,1735489879097,1735490217277,0,243,"[NULL]","[NULL]"
        11,"[NULL]","[NULL]",17,1735489933912,1735490587658,0,230,"[NULL]","[NULL]"
        "[NULL]","[NULL]","[NULL]",20,1735489972385,1735489995809,0,298,"[NULL]","[NULL]"
        "[NULL]",20,1735489995809,25,1735489999987,1735490055966,0,298,"S","[NULL]"
        25,"[NULL]","[NULL]",28,1735490039439,1735490610238,0,421,"[NULL]","[NULL]"
        25,"[NULL]","[NULL]",29,1735490042084,1735490068213,0,420,"[NULL]","[NULL]"
        25,"[NULL]","[NULL]",30,1735490045825,1735491418790,0,1,"[NULL]","[NULL]"
        17,"[NULL]","[NULL]",41,1735490544063,1735490598211,0,427,"[NULL]","[NULL]"
        """))

  def test_thread_executing_span_graph_contains_forked_states(self):
    return DiffTestBlueprint(
        trace=DataPath('sched_wakeup_trace.atr'),
        query="""
        INCLUDE PERFETTO MODULE sched.thread_executing_span;
        SELECT
          id,
          waker_id,
          prev_id
        FROM _wakeup_graph
          WHERE ts = 1735842081507 AND end_ts = 1735842081507 + 293868
        """,
        out=Csv("""
        "id","waker_id","prev_id"
        376,369,"[NULL]"
        """))

  def test_thread_executing_span_runnable_state_has_no_running(self):
    return DiffTestBlueprint(
        trace=DataPath('sched_wakeup_trace.atr'),
        query="""
        INCLUDE PERFETTO MODULE sched.thread_executing_span;
        SELECT COUNT(*) AS count FROM _runnable_state WHERE state = 'Running'
        """,
        out=Csv("""
        "count"
        0
        """))

  def test_thread_executing_span_graph_has_no_null_dur(self):
    return DiffTestBlueprint(
        trace=DataPath('sched_wakeup_trace.atr'),
        query="""
        INCLUDE PERFETTO MODULE sched.thread_executing_span;
        SELECT ts,end_ts FROM _wakeup_graph
          WHERE end_ts IS NULL OR ts IS NULL
        """,
        out=Csv("""
        "ts","end_ts"
        """))

  def test_thread_executing_span_graph_accepts_null_irq_context(self):
    return DiffTestBlueprint(
        trace=DataPath('sched_switch_original.pb'),
        query="""
        INCLUDE PERFETTO MODULE sched.thread_executing_span;
        SELECT COUNT(*) AS count FROM _wakeup_graph
        """,
        out=Csv("""
        "count"
        17
        """))

  def test_thread_executing_span_flatten_critical_path_tasks(self):
    return DiffTestBlueprint(
        trace=DataPath('sched_switch_original.pb'),
        query="""
        INCLUDE PERFETTO MODULE sched.thread_executing_span;

        CREATE PERFETTO TABLE graph AS
        SELECT
          id AS source_node_id,
          COALESCE(waker_id, id) AS dest_node_id,
          id - COALESCE(waker_id, id) AS edge_weight
        FROM _wakeup_graph;

        CREATE PERFETTO TABLE roots AS
        SELECT
          _wakeup_graph.id AS root_node_id,
          _wakeup_graph.id - COALESCE(prev_id, _wakeup_graph.id) AS root_target_weight,
          id,
          ts,
          end_ts,
          utid
        FROM _wakeup_graph LIMIT 10;

        CREATE PERFETTO TABLE critical_path AS
        SELECT * FROM graph_reachable_weight_bounded_dfs!(graph, roots, 1);

        SELECT * FROM _flatten_critical_path_tasks!(critical_path);
        """,
        out=Csv("""
        "ts","root_node_id","node_id","dur","node_utid","prev_end_ts"
        807082868359903,29,29,"[NULL]",8,"[NULL]"
        807082871734539,35,35,"[NULL]",9,"[NULL]"
        807082871734539,38,35,45052,9,"[NULL]"
        807082871779591,38,38,"[NULL]",5,807082871764903
        807082878623081,45,45,"[NULL]",9,807082871805424
        807082947156994,57,57,"[NULL]",9,807082878865945
        807082947246838,62,62,"[NULL]",6,807082879179539
        807082947261525,63,63,"[NULL]",12,"[NULL]"
        807082947267463,64,64,"[NULL]",13,"[NULL]"
        807082947278140,65,65,"[NULL]",14,"[NULL]"
        807082947288765,66,66,"[NULL]",15,"[NULL]"
        """))

  def test_thread_executing_span_intervals_to_roots_edge_case(self):
    return DiffTestBlueprint(
        trace=DataPath('sched_wakeup_trace.atr'),
        query="""
        INCLUDE PERFETTO MODULE sched.thread_executing_span;

        SELECT * FROM
        _intervals_to_roots!((SELECT 1477 AS utid, trace_start() AS ts, trace_end() - trace_start() AS dur))
        LIMIT 10;
        """,
        out=Csv("""
        "id"
        11889
        11892
        11893
        11896
        11897
        11900
        11911
        11916
        11917
        11921
        """))

  def test_thread_executing_span_intervals_to_roots(self):
    return DiffTestBlueprint(
        trace=DataPath('sched_wakeup_trace.atr'),
        query="""
        INCLUDE PERFETTO MODULE sched.thread_executing_span;

        SELECT * FROM
        _intervals_to_roots!((SELECT 1477 AS utid, 1737362149192 AS ts, CAST(2e7 AS INT) AS dur))
        LIMIT 10;
        """,
        out=Csv("""
        "id"
        11980
        11983
        11984
        11989
        11990
        11991
        11992
        11993
        12001
        12006
        """))

  def test_thread_executing_span_flatten_critical_paths(self):
    return DiffTestBlueprint(
        trace=DataPath('sched_switch_original.pb'),
        query="""
        INCLUDE PERFETTO MODULE sched.thread_executing_span;

        CREATE PERFETTO TABLE graph AS
        SELECT
          id AS source_node_id,
          COALESCE(waker_id, id) AS dest_node_id,
          id - COALESCE(waker_id, id) AS edge_weight
        FROM _wakeup_graph;

        CREATE PERFETTO TABLE roots AS
        SELECT
          _wakeup_graph.id AS root_node_id,
          _wakeup_graph.id - COALESCE(prev_id, _wakeup_graph.id) AS root_target_weight,
          id,
          ts,
          end_ts,
          utid
        FROM _wakeup_graph;

        CREATE PERFETTO TABLE critical_path AS
        SELECT * FROM graph_reachable_weight_bounded_dfs!(graph, roots, 1);

        SELECT * FROM _flatten_critical_paths!(critical_path, _sleep);
        """,
        out=Csv("""
        "ts","dur","utid","id","root_id","prev_end_ts","critical_path_utid","critical_path_id","critical_path_blocked_dur","critical_path_blocked_state","critical_path_blocked_function"
        807082871764903,14688,9,35,38,"[NULL]",5,38,14688,"S","[NULL]"
        807082947156994,351302,9,57,76,807082878865945,5,76,68858913,"S","[NULL]"
        807083031589763,324114,21,127,130,"[NULL]",5,130,80026987,"S","[NULL]"
        """))

  def test_thread_executing_span_critical_path(self):
    return DiffTestBlueprint(
        trace=DataPath('sched_switch_original.pb'),
        query="""
        INCLUDE PERFETTO MODULE sched.thread_executing_span;

        CREATE PERFETTO TABLE graph AS
        SELECT
          id AS source_node_id,
          COALESCE(waker_id, id) AS dest_node_id,
          id - COALESCE(waker_id, id) AS edge_weight
        FROM _wakeup_graph;

        CREATE PERFETTO TABLE roots AS
        SELECT
          _wakeup_graph.id AS root_node_id,
          _wakeup_graph.id - COALESCE(prev_id, _wakeup_graph.id) AS root_target_weight,
          id,
          ts,
          end_ts,
          utid
        FROM _wakeup_graph;

        SELECT * FROM _critical_path!(graph, roots, _sleep);
        """,
        out=Csv("""
        "ts","dur","root_id","id","utid","critical_path_utid","critical_path_id","critical_path_blocked_dur","critical_path_blocked_state","critical_path_blocked_function"
        807082868359903,81302,29,29,8,8,"[NULL]","[NULL]","[NULL]","[NULL]"
        807082871734539,70885,35,35,9,9,"[NULL]","[NULL]","[NULL]","[NULL]"
        807082871764903,14688,38,35,9,5,38,14688,"S","[NULL]"
        807082871779591,55729,38,38,5,5,"[NULL]","[NULL]","[NULL]","[NULL]"
        807082878623081,242864,45,45,9,9,"[NULL]","[NULL]","[NULL]","[NULL]"
        807082947156994,436354,57,57,9,9,"[NULL]","[NULL]","[NULL]","[NULL]"
        807082947246838,1038854,62,62,6,6,"[NULL]","[NULL]","[NULL]","[NULL]"
        807082947261525,293594,63,63,12,12,"[NULL]","[NULL]","[NULL]","[NULL]"
        807082947267463,228958,64,64,13,13,"[NULL]","[NULL]","[NULL]","[NULL]"
        807082947278140,54114,65,65,14,14,"[NULL]","[NULL]","[NULL]","[NULL]"
        807082947288765,338802,66,66,15,15,"[NULL]","[NULL]","[NULL]","[NULL]"
        807082947294182,296875,67,67,16,16,"[NULL]","[NULL]","[NULL]","[NULL]"
        807082947156994,351302,76,57,9,5,76,68858913,"S","[NULL]"
        807082947508296,122083,76,76,5,5,"[NULL]","[NULL]","[NULL]","[NULL]"
        807082951822463,104427,96,96,9,9,"[NULL]","[NULL]","[NULL]","[NULL]"
        807082959173506,215104,107,107,6,6,"[NULL]","[NULL]","[NULL]","[NULL]"
        807083031589763,436198,127,127,21,21,"[NULL]","[NULL]","[NULL]","[NULL]"
        807083031589763,324114,130,127,21,5,130,80026987,"S","[NULL]"
        807083031913877,166302,130,130,5,5,"[NULL]","[NULL]","[NULL]","[NULL]"
        807083032278825,208490,135,135,2,2,"[NULL]","[NULL]","[NULL]","[NULL]"
        """))

  def test_thread_executing_span_critical_path_by_roots(self):
    return DiffTestBlueprint(
        trace=DataPath('sched_switch_original.pb'),
        query="""
        INCLUDE PERFETTO MODULE sched.thread_executing_span;

        SELECT * FROM _critical_path_by_roots!(_intervals_to_roots!((SELECT 6 AS utid, trace_start() AS ts, trace_end() - trace_start() AS dur)));
        """,
        out=Csv("""
        "id","ts","dur","utid","critical_path_id","critical_path_blocked_dur","critical_path_blocked_state","critical_path_blocked_function","critical_path_utid"
        62,807082947246838,1038854,6,"[NULL]","[NULL]","[NULL]","[NULL]",6
        63,807082947261525,293594,12,"[NULL]","[NULL]","[NULL]","[NULL]",12
        64,807082947267463,228958,13,"[NULL]","[NULL]","[NULL]","[NULL]",13
        65,807082947278140,54114,14,"[NULL]","[NULL]","[NULL]","[NULL]",14
        66,807082947288765,338802,15,"[NULL]","[NULL]","[NULL]","[NULL]",15
        67,807082947294182,296875,16,"[NULL]","[NULL]","[NULL]","[NULL]",16
        57,807082947156994,351302,9,76,68858913,"S","[NULL]",5
        76,807082947508296,122083,5,"[NULL]","[NULL]","[NULL]","[NULL]",5
        96,807082951822463,104427,9,"[NULL]","[NULL]","[NULL]","[NULL]",9
        107,807082959173506,215104,6,"[NULL]","[NULL]","[NULL]","[NULL]",6
        """))

  def test_thread_executing_span_critical_path_by_intervals(self):
    return DiffTestBlueprint(
        trace=DataPath('sched_switch_original.pb'),
        query="""
        INCLUDE PERFETTO MODULE sched.thread_executing_span;

        SELECT * FROM _critical_path_by_intervals!((SELECT 6 AS utid, trace_start() AS ts, trace_end() - trace_start() AS dur));
        """,
        out=Csv("""
        "id","ts","dur","utid","critical_path_id","critical_path_blocked_dur","critical_path_blocked_state","critical_path_blocked_function","critical_path_utid"
        62,807082947246838,1038854,6,"[NULL]","[NULL]","[NULL]","[NULL]",6
        107,807082959173506,215104,6,"[NULL]","[NULL]","[NULL]","[NULL]",6
        """))

  def test_thread_executing_span_critical_path_utid(self):
    return DiffTestBlueprint(
        trace=DataPath('sched_wakeup_trace.atr'),
        query="""
        INCLUDE PERFETTO MODULE sched.thread_executing_span;
        SELECT
          id,
          ts,
          dur,
          utid,
          critical_path_id,
          critical_path_blocked_dur,
          critical_path_blocked_state,
          critical_path_blocked_function,
          critical_path_utid
        FROM _thread_executing_span_critical_path((select utid from thread where tid = 3487), start_ts, end_ts), trace_bounds
        ORDER BY ts
        LIMIT 10
        """,
        out=Csv("""
        "id","ts","dur","utid","critical_path_id","critical_path_blocked_dur","critical_path_blocked_state","critical_path_blocked_function","critical_path_utid"
        11889,1737349401439,7705561,1477,"[NULL]","[NULL]","[NULL]","[NULL]",1477
        11952,1737357107000,547583,1480,11980,547583,"S","[NULL]",1477
        11980,1737357654583,8430762,1477,"[NULL]","[NULL]","[NULL]","[NULL]",1477
        12052,1737366085345,50400,91,12057,50400,"S","[NULL]",1477
        12057,1737366135745,6635927,1477,"[NULL]","[NULL]","[NULL]","[NULL]",1477
        12081,1737372771672,12798314,1488,12254,12798314,"S","[NULL]",1477
        12254,1737385569986,21830622,1477,"[NULL]","[NULL]","[NULL]","[NULL]",1477
        12517,1737407400608,241267,91,12521,241267,"S","[NULL]",1477
        12521,1737407641875,1830015,1477,"[NULL]","[NULL]","[NULL]","[NULL]",1477
        12669,1737409471890,68590,91,12672,68590,"S","[NULL]",1477
        """))

  def test_thread_executing_span_critical_path_stack(self):
    return DiffTestBlueprint(
        trace=DataPath('sched_wakeup_trace.atr'),
        query="""
        INCLUDE PERFETTO MODULE sched.thread_executing_span_with_slice;
        SELECT
          id,
          ts,
          dur,
          utid,
          stack_depth,
          name,
          table_name,
          critical_path_utid
        FROM _thread_executing_span_critical_path_stack((select utid from thread where tid = 3487), start_ts, end_ts), trace_bounds
        ORDER BY ts
        LIMIT 11
        """,
        out=Csv("""
        "id","ts","dur","utid","stack_depth","name","table_name","critical_path_utid"
        11889,1737349401439,57188,1477,0,"thread_state: R","thread_state",1477
        11889,1737349401439,57188,1477,1,"[NULL]","thread_state",1477
        11889,1737349401439,57188,1477,2,"[NULL]","thread_state",1477
        11889,1737349401439,57188,1477,3,"process_name: com.android.providers.media.module","thread_state",1477
        11889,1737349401439,57188,1477,4,"thread_name: rs.media.module","thread_state",1477
        11891,1737349458627,1884896,1477,0,"thread_state: Running","thread_state",1477
        11891,1737349458627,1884896,1477,1,"[NULL]","thread_state",1477
        11891,1737349458627,1884896,1477,2,"[NULL]","thread_state",1477
        11891,1737349458627,1884896,1477,3,"process_name: com.android.providers.media.module","thread_state",1477
        11891,1737349458627,1884896,1477,4,"thread_name: rs.media.module","thread_state",1477
        11891,1737349458627,1884896,1477,5,"cpu: 0","thread_state",1477
        """))

  def test_thread_executing_span_critical_path_graph(self):
    return DiffTestBlueprint(
        trace=DataPath('sched_wakeup_trace.atr'),
        query="""
        INCLUDE PERFETTO MODULE sched.thread_executing_span_with_slice;
        SELECT HEX(pprof) FROM _thread_executing_span_critical_path_graph("critical path", (select utid from thread where tid = 3487), 1737488133487, 16000), trace_bounds
      """,
        out=BinaryProto(
            message_type="perfetto.third_party.perftools.profiles.Profile",
            post_processing=PrintProfileProto,
            contents="""
        Sample:
        Values: 0
        Stack:
        bindApplication (0x0)
        thread_name: rs.media.module (0x0)
        process_name: com.android.providers.media.module (0x0)
        thread_state: R (0x0)
        critical path (0x0)

        Sample:
        Values: 0
        Stack:
        bindApplication (0x0)
        thread_name: rs.media.module (0x0)
        process_name: com.android.providers.media.module (0x0)
        thread_state: S (0x0)
        critical path (0x0)

        Sample:
        Values: 0
        Stack:
        binder reply (0x0)
        blocking thread_name: binder:553_3 (0x0)
        blocking process_name: /system/bin/mediaserver (0x0)
        blocking thread_state: Running (0x0)
        binder transaction (0x0)
        bindApplication (0x0)
        thread_name: rs.media.module (0x0)
        process_name: com.android.providers.media.module (0x0)
        thread_state: S (0x0)
        critical path (0x0)

        Sample:
        Values: 0
        Stack:
        binder transaction (0x0)
        bindApplication (0x0)
        thread_name: rs.media.module (0x0)
        process_name: com.android.providers.media.module (0x0)
        thread_state: S (0x0)
        critical path (0x0)

        Sample:
        Values: 0
        Stack:
        blocking process_name: /system/bin/mediaserver (0x0)
        blocking thread_state: Running (0x0)
        binder transaction (0x0)
        bindApplication (0x0)
        thread_name: rs.media.module (0x0)
        process_name: com.android.providers.media.module (0x0)
        thread_state: S (0x0)
        critical path (0x0)

        Sample:
        Values: 0
        Stack:
        blocking thread_name: binder:553_3 (0x0)
        blocking process_name: /system/bin/mediaserver (0x0)
        blocking thread_state: Running (0x0)
        binder transaction (0x0)
        bindApplication (0x0)
        thread_name: rs.media.module (0x0)
        process_name: com.android.providers.media.module (0x0)
        thread_state: S (0x0)
        critical path (0x0)

        Sample:
        Values: 0
        Stack:
        blocking thread_state: Running (0x0)
        binder transaction (0x0)
        bindApplication (0x0)
        thread_name: rs.media.module (0x0)
        process_name: com.android.providers.media.module (0x0)
        thread_state: S (0x0)
        critical path (0x0)

        Sample:
        Values: 0
        Stack:
        process_name: com.android.providers.media.module (0x0)
        thread_state: R (0x0)
        critical path (0x0)

        Sample:
        Values: 0
        Stack:
        process_name: com.android.providers.media.module (0x0)
        thread_state: S (0x0)
        critical path (0x0)

        Sample:
        Values: 0
        Stack:
        thread_name: rs.media.module (0x0)
        process_name: com.android.providers.media.module (0x0)
        thread_state: R (0x0)
        critical path (0x0)

        Sample:
        Values: 0
        Stack:
        thread_name: rs.media.module (0x0)
        process_name: com.android.providers.media.module (0x0)
        thread_state: S (0x0)
        critical path (0x0)

        Sample:
        Values: 0
        Stack:
        thread_state: R (0x0)
        critical path (0x0)

        Sample:
        Values: 0
        Stack:
        thread_state: S (0x0)
        critical path (0x0)

        Sample:
        Values: 1101
        Stack:
        binder transaction (0x0)
        bindApplication (0x0)
        thread_name: rs.media.module (0x0)
        process_name: com.android.providers.media.module (0x0)
        thread_state: R (0x0)
        critical path (0x0)

        Sample:
        Values: 13010
        Stack:
        cpu: 0 (0x0)
        binder reply (0x0)
        blocking thread_name: binder:553_3 (0x0)
        blocking process_name: /system/bin/mediaserver (0x0)
        blocking thread_state: Running (0x0)
        binder transaction (0x0)
        bindApplication (0x0)
        thread_name: rs.media.module (0x0)
        process_name: com.android.providers.media.module (0x0)
        thread_state: S (0x0)
        critical path (0x0)

        Sample:
        Values: 1889
        Stack:
        cpu: 0 (0x0)
        blocking thread_name: binder:553_3 (0x0)
        blocking process_name: /system/bin/mediaserver (0x0)
        blocking thread_state: Running (0x0)
        binder transaction (0x0)
        bindApplication (0x0)
        thread_name: rs.media.module (0x0)
        process_name: com.android.providers.media.module (0x0)
        thread_state: S (0x0)
        critical path (0x0)
        """))
