// Copyright (C) 2023 The Android Open Source Project
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

import {NamedSliceTrackTypes} from '../named_slice_track';
import {
  CustomSqlDetailsPanelConfig,
  CustomSqlTableDefConfig,
  CustomSqlTableSliceTrack,
} from '../tracks/custom_sql_table_slice_track';
import {TrackContext} from '../../public';
import {Engine} from '../../trace_processor/engine';
import {DebugSliceDetailsTab} from './details_tab';

export class DebugSliceTrack extends CustomSqlTableSliceTrack<NamedSliceTrackTypes> {
  private readonly sqlTableName: string;

  constructor(engine: Engine, ctx: TrackContext, tableName: string) {
    super({
      engine,
      trackKey: ctx.trackKey,
    });
    this.sqlTableName = tableName;
  }

  async getSqlDataSource(): Promise<CustomSqlTableDefConfig> {
    return {
      sqlTableName: this.sqlTableName,
    };
  }

  getDetailsPanel(): CustomSqlDetailsPanelConfig {
    return {
      kind: DebugSliceDetailsTab.kind,
      config: {
        sqlTableName: this.sqlTableName,
        title: 'Debug Slice',
      },
    };
  }
}
