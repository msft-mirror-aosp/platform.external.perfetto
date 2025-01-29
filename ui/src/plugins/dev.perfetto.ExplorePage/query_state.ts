// Copyright (C) 2025 The Android Open Source Project
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

import protos from '../../protos';
import {SqlTable} from '../dev.perfetto.SqlModules/sql_modules';
import {ColumnControllerRows} from './query_builder/column_controller';

export enum NodeType {
  kStdlibTable,
  kJoinOperator,
}

export interface QueryNode {
  readonly type: NodeType;
  readonly prevNode?: QueryNode;
  nextNode?: QueryNode;
  finished: boolean;

  dataName?: string;
  cte: boolean;
  imports?: string[];
  columns?: ColumnControllerRows[];

  getSourceSql(): string | undefined;
  getTitle(): string;
  getStructuredQuery(): protos.PerfettoSqlStructuredQuery | undefined;

  validate(): boolean;
}

export class StdlibTableState implements QueryNode {
  readonly type: NodeType = NodeType.kStdlibTable;
  prevNode = undefined;
  nextNode?: QueryNode;
  finished: boolean = true;

  dataName?: string;
  cte = false;
  imports: string[];
  columns: ColumnControllerRows[];

  sqlTable: SqlTable;

  getSourceSql(): string | undefined {
    return `${this.sqlTable.name}`;
  }
  getTitle(): string {
    return `Table ${this.sqlTable.name}`;
  }
  validate(): boolean {
    return true;
  }
  getStructuredQuery(): protos.PerfettoSqlStructuredQuery | undefined {
    if (!this.validate()) return;

    const sq = new protos.PerfettoSqlStructuredQuery();
    sq.id = `table_source_${this.sqlTable.name}`;
    sq.table = new protos.PerfettoSqlStructuredQuery.Table();
    sq.table.tableName = this.sqlTable.name;
    sq.table.moduleName = this.sqlTable.includeKey
      ? this.sqlTable.includeKey
      : undefined;
    sq.table.columnNames = this.columns
      .filter((c) => c.checked)
      .map((c) => c.column.name);

    const selectedColumns: protos.PerfettoSqlStructuredQuery.SelectColumn[] =
      [];
    for (const c of this.columns.filter((c) => c.checked)) {
      const newC = new protos.PerfettoSqlStructuredQuery.SelectColumn();
      newC.columnName = c.column.name;
      if (c.alias) {
        newC.alias = c.alias;
      }
      selectedColumns.push(newC);
    }
    sq.selectColumns = selectedColumns;
    return sq;
  }

  constructor(sqlTable: SqlTable) {
    this.dataName = sqlTable.name;
    this.imports = sqlTable.includeKey ? [sqlTable.includeKey] : [];
    this.columns = sqlTable.columns.map((c) => new ColumnControllerRows(c));
    this.sqlTable = sqlTable;
  }
}

export function getLastFinishedNode(node: QueryNode): QueryNode | undefined {
  if (!node.finished) {
    return;
  }
  while (node.nextNode) {
    if (!node.nextNode.finished) {
      return node;
    }
    node = node.nextNode;
  }
  return node;
}

export function getFirstNode(node: QueryNode): QueryNode | undefined {
  if (!node.finished) {
    return;
  }
  while (node.prevNode) {
    if (!node.finished) {
      return;
    }
    node = node.prevNode;
  }
  return node;
}
