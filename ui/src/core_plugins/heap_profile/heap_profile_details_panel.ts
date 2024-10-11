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

import m from 'mithril';
import {assertExists, assertFalse} from '../../base/logging';
import {time} from '../../base/time';
import {Actions} from '../../common/actions';
import {
  QueryFlamegraph,
  QueryFlamegraphAttrs,
  metricsFromTableOrSubquery,
} from '../../core/query_flamegraph';
import {raf} from '../../core/raf_scheduler';
import {Router} from '../../core/router';
import {globals} from '../../frontend/globals';
import {getCurrentTrace} from '../../frontend/sidebar';
import {convertTraceToPprofAndDownload} from '../../frontend/trace_converter';
import {Timestamp} from '../../frontend/widgets/timestamp';
import {TrackEventDetailsPanel} from '../../public/details_panel';
import {ProfileType, TrackEventSelection} from '../../public/selection';
import {Trace} from '../../public/trace';
import {Engine} from '../../trace_processor/engine';
import {NUM} from '../../trace_processor/query_result';
import {Button} from '../../widgets/button';
import {Intent} from '../../widgets/common';
import {DetailsShell} from '../../widgets/details_shell';
import {Icon} from '../../widgets/icon';
import {Modal} from '../../widgets/modal';
import {Popup} from '../../widgets/popup';

interface Props {
  ts: time;
  type: ProfileType;
}

export class HeapProfileFlamegraphDetailsPanel
  implements TrackEventDetailsPanel
{
  private flamegraphAttrs?: QueryFlamegraphAttrs;
  private props?: Props;

  constructor(
    private trace: Trace,
    private heapGraphIncomplete: boolean,
    private upid: number,
  ) {}

  async load(sel: TrackEventSelection) {
    const {profileType, ts} = sel;

    this.flamegraphAttrs = flamegraphAttrs(
      this.trace.engine,
      ts,
      this.upid,
      assertExists(profileType),
    );

    this.props = {ts, type: assertExists(profileType)};
  }

  render() {
    if (!this.props) {
      return undefined;
    }

    const {type, ts} = this.props;

    return m(
      '.flamegraph-profile',
      maybeShowModal(type, this.heapGraphIncomplete),
      m(
        DetailsShell,
        {
          fillParent: true,
          title: m(
            '.title',
            getFlamegraphTitle(type),
            type === ProfileType.MIXED_HEAP_PROFILE &&
              m(
                Popup,
                {
                  trigger: m(Icon, {icon: 'warning'}),
                },
                m(
                  '',
                  {style: {width: '300px'}},
                  'This is a mixed java/native heap profile, free()s are not visualized. To visualize free()s, remove "all_heaps: true" from the config.',
                ),
              ),
          ),
          description: [],
          buttons: [
            m('.time', `Snapshot time: `, m(Timestamp, {ts})),
            (type === ProfileType.NATIVE_HEAP_PROFILE ||
              type === ProfileType.JAVA_HEAP_SAMPLES) &&
              m(Button, {
                icon: 'file_download',
                intent: Intent.Primary,
                onclick: () => {
                  downloadPprof(this.trace.engine, this.upid, ts);
                  raf.scheduleFullRedraw();
                },
              }),
          ],
        },
        m(QueryFlamegraph, assertExists(this.flamegraphAttrs)),
      ),
    );
  }
}

function flamegraphAttrs(
  engine: Engine,
  ts: time,
  upid: number,
  type: ProfileType,
): QueryFlamegraphAttrs {
  switch (type) {
    case ProfileType.NATIVE_HEAP_PROFILE:
      return flamegraphAttrsForHeapProfile(engine, ts, upid, [
        {
          name: 'Unreleased Malloc Size',
          unit: 'B',
          columnName: 'self_size',
        },
        {
          name: 'Unreleased Malloc Count',
          unit: '',
          columnName: 'self_count',
        },
        {
          name: 'Total Malloc Size',
          unit: 'B',
          columnName: 'self_alloc_size',
        },
        {
          name: 'Total Malloc Count',
          unit: '',
          columnName: 'self_alloc_count',
        },
      ]);
    case ProfileType.HEAP_PROFILE:
      return flamegraphAttrsForHeapProfile(engine, ts, upid, [
        {
          name: 'Unreleased Size',
          unit: 'B',
          columnName: 'self_size',
        },
        {
          name: 'Unreleased Count',
          unit: '',
          columnName: 'self_count',
        },
        {
          name: 'Total Size',
          unit: 'B',
          columnName: 'self_alloc_size',
        },
        {
          name: 'Total Count',
          unit: '',
          columnName: 'self_alloc_count',
        },
      ]);
    case ProfileType.JAVA_HEAP_SAMPLES:
      return flamegraphAttrsForHeapProfile(engine, ts, upid, [
        {
          name: 'Unreleased Allocation Size',
          unit: 'B',
          columnName: 'self_size',
        },
        {
          name: 'Unreleased Allocation Count',
          unit: '',
          columnName: 'self_count',
        },
      ]);
    case ProfileType.MIXED_HEAP_PROFILE:
      return flamegraphAttrsForHeapProfile(engine, ts, upid, [
        {
          name: 'Unreleased Allocation Size (malloc + java)',
          unit: 'B',
          columnName: 'self_size',
        },
        {
          name: 'Unreleased Allocation Count (malloc + java)',
          unit: '',
          columnName: 'self_count',
        },
      ]);
    case ProfileType.JAVA_HEAP_GRAPH:
      return flamegraphAttrsForHeapGraph(engine, ts, upid);
    case ProfileType.PERF_SAMPLE:
      assertFalse(false, 'Perf sample not supported');
      return {engine, metrics: []};
  }
}

function flamegraphAttrsForHeapProfile(
  engine: Engine,
  ts: time,
  upid: number,
  metrics: {name: string; unit: string; columnName: string}[],
) {
  return {
    engine,
    metrics: [
      ...metricsFromTableOrSubquery(
        `
          (
            select
              id,
              parent_id as parentId,
              name,
              mapping_name,
              source_file,
              cast(line_number AS text) as line_number,
              self_size,
              self_count,
              self_alloc_size,
              self_alloc_count
            from _android_heap_profile_callstacks_for_allocations!((
              select
                callsite_id,
                size,
                count,
                max(size, 0) as alloc_size,
                max(count, 0) as alloc_count
              from heap_profile_allocation a
              where a.ts <= ${ts} and a.upid = ${upid}
            ))
          )
        `,
        metrics,
        'include perfetto module android.memory.heap_profile.callstacks',
        [{name: 'mapping_name', displayName: 'Mapping'}],
        [
          {
            name: 'source_file',
            displayName: 'Source File',
            mergeAggregation: 'ONE_OR_NULL',
          },
          {
            name: 'line_number',
            displayName: 'Line Number',
            mergeAggregation: 'ONE_OR_NULL',
          },
        ],
      ),
    ],
  };
}

function flamegraphAttrsForHeapGraph(
  engine: Engine,
  ts: time,
  upid: number,
): QueryFlamegraphAttrs {
  return {
    engine,
    metrics: [
      {
        name: 'Object Size',
        unit: 'B',
        dependencySql:
          'include perfetto module android.memory.heap_graph.class_tree;',
        statement: `
          select
            id,
            parent_id as parentId,
            ifnull(name, '[Unknown]') as name,
            root_type,
            self_size as value,
            self_count
          from _heap_graph_class_tree
          where graph_sample_ts = ${ts} and upid = ${upid}
        `,
        unaggregatableProperties: [
          {name: 'root_type', displayName: 'Root Type'},
        ],
        aggregatableProperties: [
          {
            name: 'self_count',
            displayName: 'Self Count',
            mergeAggregation: 'SUM',
          },
        ],
      },
      {
        name: 'Object Count',
        unit: '',
        dependencySql:
          'include perfetto module android.memory.heap_graph.class_tree;',
        statement: `
          select
            id,
            parent_id as parentId,
            ifnull(name, '[Unknown]') as name,
            root_type,
            self_size,
            self_count as value
          from _heap_graph_class_tree
          where graph_sample_ts = ${ts} and upid = ${upid}
        `,
        unaggregatableProperties: [
          {name: 'root_type', displayName: 'Root Type'},
        ],
      },
      {
        name: 'Dominated Object Size',
        unit: 'B',
        dependencySql:
          'include perfetto module android.memory.heap_graph.dominator_class_tree;',
        statement: `
          select
            id,
            parent_id as parentId,
            ifnull(name, '[Unknown]') as name,
            root_type,
            self_size as value,
            self_count
          from _heap_graph_dominator_class_tree
          where graph_sample_ts = ${ts} and upid = ${upid}
        `,
        unaggregatableProperties: [
          {name: 'root_type', displayName: 'Root Type'},
        ],
        aggregatableProperties: [
          {
            name: 'self_count',
            displayName: 'Self Count',
            mergeAggregation: 'SUM',
          },
        ],
      },
      {
        name: 'Dominated Object Count',
        unit: '',
        dependencySql:
          'include perfetto module android.memory.heap_graph.dominator_class_tree;',
        statement: `
          select
            id,
            parent_id as parentId,
            ifnull(name, '[Unknown]') as name,
            root_type,
            self_size,
            self_count as value
          from _heap_graph_class_tree
          where graph_sample_ts = ${ts} and upid = ${upid}
        `,
        unaggregatableProperties: [
          {name: 'root_type', displayName: 'Root Type'},
        ],
      },
    ],
  };
}

function getFlamegraphTitle(type: ProfileType) {
  switch (type) {
    case ProfileType.HEAP_PROFILE:
      return 'Heap profile';
    case ProfileType.JAVA_HEAP_GRAPH:
      return 'Java heap graph';
    case ProfileType.JAVA_HEAP_SAMPLES:
      return 'Java heap samples';
    case ProfileType.MIXED_HEAP_PROFILE:
      return 'Mixed heap profile';
    case ProfileType.NATIVE_HEAP_PROFILE:
      return 'Native heap profile';
    case ProfileType.PERF_SAMPLE:
      assertFalse(false, 'Perf sample not supported');
      return 'Impossible';
  }
}

async function downloadPprof(
  engine: Engine | undefined,
  upid: number,
  ts: time,
) {
  if (engine === undefined) {
    return;
  }
  try {
    const pid = await engine.query(
      `select pid from process where upid = ${upid}`,
    );
    const trace = await getCurrentTrace();
    convertTraceToPprofAndDownload(trace, pid.firstRow({pid: NUM}).pid, ts);
  } catch (error) {
    throw new Error(`Failed to get current trace ${error}`);
  }
}

function maybeShowModal(type: ProfileType, heapGraphIncomplete: boolean) {
  if (type !== ProfileType.JAVA_HEAP_GRAPH || !heapGraphIncomplete) {
    return undefined;
  }
  if (globals.state.flamegraphModalDismissed) {
    return undefined;
  }
  return m(Modal, {
    title: 'The flamegraph is incomplete',
    vAlign: 'TOP',
    content: m(
      'div',
      'The current trace does not have a fully formed flamegraph',
    ),
    buttons: [
      {
        text: 'Show the errors',
        primary: true,
        action: () => Router.navigate('#!/info'),
      },
      {
        text: 'Skip',
        action: () => {
          globals.dispatch(Actions.dismissFlamegraphModal({}));
          raf.scheduleFullRedraw();
        },
      },
    ],
  });
}