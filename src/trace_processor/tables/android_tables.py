# Copyright (C) 2022 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Contains tables for relevant for Android."""

from python.generators.trace_processor_table.public import Column as C
from python.generators.trace_processor_table.public import ColumnDoc
from python.generators.trace_processor_table.public import CppDouble
from python.generators.trace_processor_table.public import CppInt32
from python.generators.trace_processor_table.public import CppInt64
from python.generators.trace_processor_table.public import CppOptional
from python.generators.trace_processor_table.public import CppString
from python.generators.trace_processor_table.public import Table
from python.generators.trace_processor_table.public import TableDoc
from python.generators.trace_processor_table.public import CppTableId
from python.generators.trace_processor_table.public import CppUint32
from python.generators.trace_processor_table.public import WrappingSqlView

from src.trace_processor.tables.metadata_tables import THREAD_TABLE

ANDROID_LOG_TABLE = Table(
    python_module=__file__,
    class_name="AndroidLogTable",
    sql_name="android_logs",
    columns=[
        C("ts", CppInt64()),
        C("utid", CppTableId(THREAD_TABLE)),
        C("prio", CppUint32()),
        C("tag", CppOptional(CppString())),
        C("msg", CppString()),
    ],
    tabledoc=TableDoc(
        doc='''
          Log entries from Android logcat.

          NOTE: this table is not sorted by timestamp. This is why we omit the
          sorted flag on the ts column.
        ''',
        group='Android',
        columns={
            'ts': 'Timestamp of log entry.',
            'utid': 'Thread writing the log entry.',
            'prio': 'Priority of the log. 3=DEBUG, 4=INFO, 5=WARN, 6=ERROR.',
            'tag': 'Tag of the log entry.',
            'msg': 'Content of the log entry.'
        }))

ANDROID_GAME_INTERVENTION_LIST_TABLE = Table(
    python_module=__file__,
    class_name='AndroidGameInterventionListTable',
    sql_name='android_game_intervention_list',
    columns=[
        C('package_name', CppString()),
        C('uid', CppInt64()),
        C('current_mode', CppInt32()),
        C('standard_mode_supported', CppInt32()),
        C('standard_mode_downscale', CppOptional(CppDouble())),
        C('standard_mode_use_angle', CppOptional(CppInt32())),
        C('standard_mode_fps', CppOptional(CppDouble())),
        C('perf_mode_supported', CppInt32()),
        C('perf_mode_downscale', CppOptional(CppDouble())),
        C('perf_mode_use_angle', CppOptional(CppInt32())),
        C('perf_mode_fps', CppOptional(CppDouble())),
        C('battery_mode_supported', CppInt32()),
        C('battery_mode_downscale', CppOptional(CppDouble())),
        C('battery_mode_use_angle', CppOptional(CppInt32())),
        C('battery_mode_fps', CppOptional(CppDouble())),
    ],
    tabledoc=TableDoc(
        doc='''
          A table presenting all game modes and interventions
          of games installed on the system.
          This is generated by the game_mode_intervention data-source.
        ''',
        group='Android',
        columns={
            'package_name':
                '''name of the pakcage, e.g. com.google.android.gm.''',
            'uid':
                '''UID processes of this package runs as.''',
            'current_mode':
                '''current game mode the game is running at.''',
            'standard_mode_supported':
                '''bool whether standard mode is supported.''',
            'standard_mode_downscale':
                '''
                    resolution downscaling factor of standard
                    mode.
                ''',
            'standard_mode_use_angle':
                '''bool whether ANGLE is used in standard mode.''',
            'standard_mode_fps':
                '''
                    frame rate that the game is throttled at in standard
                    mode.
                ''',
            'perf_mode_supported':
                '''bool whether performance mode is supported.''',
            'perf_mode_downscale':
                '''resolution downscaling factor of performance mode.''',
            'perf_mode_use_angle':
                '''bool whether ANGLE is used in performance mode.''',
            'perf_mode_fps':
                '''
                    frame rate that the game is throttled at in performance
                    mode.
                ''',
            'battery_mode_supported':
                '''bool whether battery mode is supported.''',
            'battery_mode_downscale':
                '''resolution downscaling factor of battery mode.''',
            'battery_mode_use_angle':
                '''bool whether ANGLE is used in battery mode.''',
            'battery_mode_fps':
                '''
                    frame rate that the game is throttled at in battery
                    mode.
                '''
        }))

ANDROID_DUMPSTATE_TABLE = Table(
    python_module=__file__,
    class_name='AndroidDumpstateTable',
    sql_name='android_dumpstate',
    columns=[
        C('section', CppOptional(CppString())),
        C('service', CppOptional(CppString())),
        C('line', CppString()),
    ],
    tabledoc=TableDoc(
        doc='''
          Dumpsys entries from Android dumpstate.
        ''',
        group='Android',
        columns={
            'section':
                '''name of the dumpstate section.''',
            'service':
                '''
                    name of the dumpsys service. Only present when
                    dumpstate=="dumpsys", NULL otherwise.
                ''',
            'line':
                '''
                    line-by-line contents of the section/service,
                    one row per line.
                '''
        }))

ANDROID_MOTION_EVENTS_TABLE = Table(
    python_module=__file__,
    class_name='AndroidMotionEventsTable',
    sql_name='__intrinsic_android_motion_events',
    columns=[
        C('event_id', CppUint32()),
        C('ts', CppInt64()),
        C('arg_set_id', CppUint32()),
    ],
    tabledoc=TableDoc(
        doc='Contains Android MotionEvents processed by the system',
        group='Android',
        columns={
            'event_id':
                '''
                    The randomly-generated ID associated with each input event processed
                    by Android Framework, used to track the event through the input pipeline.
                ''',
            'ts':
                '''The timestamp of when the input event was processed by the system.''',
            'arg_set_id':
                ColumnDoc(
                    doc='Details of the motion event parsed from the proto message.',
                    joinable='args.arg_set_id'),
        }))

ANDROID_KEY_EVENTS_TABLE = Table(
    python_module=__file__,
    class_name='AndroidKeyEventsTable',
    sql_name='__intrinsic_android_key_events',
    columns=[
        C('event_id', CppUint32()),
        C('ts', CppInt64()),
        C('arg_set_id', CppUint32()),
    ],
    tabledoc=TableDoc(
        doc='Contains Android KeyEvents processed by the system',
        group='Android',
        columns={
            'event_id':
                '''
                    The randomly-generated ID associated with each input event processed
                    by Android Framework, used to track the event through the input pipeline.
                ''',
            'ts':
                '''The timestamp of when the input event was processed by the system.''',
            'arg_set_id':
                ColumnDoc(
                    doc='Details of the key event parsed from the proto message.',
                    joinable='args.arg_set_id'),
        }))

ANDROID_INPUT_EVENT_DISPATCH_TABLE = Table(
    python_module=__file__,
    class_name='AndroidInputEventDispatchTable',
    sql_name='__intrinsic_android_input_event_dispatch',
    columns=[
        C('event_id', CppUint32()),
        C('arg_set_id', CppUint32()),
        C('vsync_id', CppInt64()),
        C('window_id', CppInt32()),
    ],
    tabledoc=TableDoc(
        doc='''
                Contains records of Android input events being dispatched to input windows
                by the Android Framework.
            ''',
        group='Android',
        columns={
            'event_id':
                ColumnDoc(
                    doc='The id of the input event that was dispatched.',
                    joinable='__intrinsic_android_motion_events.event_id'),
            'arg_set_id':
                ColumnDoc(
                    doc='Details of the dispatched event parsed from the proto message.',
                    joinable='args.arg_set_id'),
            'vsync_id':
                '''
                    The id of the vsync during which the Framework made the decision to
                    dispatch this input event, used to identify the state of the input windows
                    when the dispatching decision was made.
                ''',
            'window_id':
                'The id of the window to which the event was dispatched.',
        }))

# Keep this list sorted.
ALL_TABLES = [
    ANDROID_LOG_TABLE,
    ANDROID_DUMPSTATE_TABLE,
    ANDROID_GAME_INTERVENTION_LIST_TABLE,
    ANDROID_KEY_EVENTS_TABLE,
    ANDROID_MOTION_EVENTS_TABLE,
    ANDROID_INPUT_EVENT_DISPATCH_TABLE,
]
