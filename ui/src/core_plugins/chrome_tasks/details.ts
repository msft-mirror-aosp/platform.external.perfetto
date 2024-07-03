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

import {BottomTab, NewBottomTabArgs} from '../../frontend/bottom_tab';
import {GenericSliceDetailsTabConfig} from '../../frontend/generic_slice_details_tab';
import {Details, DetailsSchema} from '../../frontend/sql/details/details';
import {wellKnownTypes} from '../../frontend/sql/details/well_known_types';
import {DetailsShell} from '../../widgets/details_shell';
import {GridLayout, GridLayoutColumn} from '../../widgets/grid_layout';

import d = DetailsSchema;

export class ChromeTasksDetailsTab extends BottomTab<GenericSliceDetailsTabConfig> {
  static readonly kind = 'org.chromium.ChromeTasks.TaskDetailsTab';

  private data: Details;

  constructor(args: NewBottomTabArgs<GenericSliceDetailsTabConfig>) {
    super(args);

    this.data = new Details(
      this.engine,
      'chrome_tasks',
      this.config.id,
      {
        'Task name': 'name',
        'Start time': d.Timestamp('ts'),
        'Duration': d.Interval('ts', 'dur'),
        'Process': d.SqlIdRef('process', 'upid'),
        'Thread': d.SqlIdRef('thread', 'utid'),
        'Slice': d.SqlIdRef('slice', 'id'),
      },
      wellKnownTypes,
    );
  }

  viewTab() {
    return m(
      DetailsShell,
      {
        title: this.getTitle(),
      },
      m(GridLayout, m(GridLayoutColumn, this.data.render())),
    );
  }

  getTitle(): string {
    return this.config.title;
  }

  isLoading() {
    return this.data.isLoading();
  }
}
