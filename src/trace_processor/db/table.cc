/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/db/table.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "perfetto/base/logging.h"
#include "perfetto/public/compiler.h"
#include "perfetto/trace_processor/ref_counted.h"
#include "src/trace_processor/containers/row_map.h"
#include "src/trace_processor/containers/string_pool.h"
#include "src/trace_processor/db/column.h"
#include "src/trace_processor/db/column/arrangement_overlay.h"
#include "src/trace_processor/db/column/data_layer.h"
#include "src/trace_processor/db/column/range_overlay.h"
#include "src/trace_processor/db/column/selector_overlay.h"
#include "src/trace_processor/db/column/types.h"
#include "src/trace_processor/db/column_storage_overlay.h"
#include "src/trace_processor/db/query_executor.h"

namespace perfetto::trace_processor {

namespace {
using Indices = column::DataLayerChain::Indices;

static constexpr uint32_t kIndexVectorThreshold = 1024;

// Returns if |op| is an operation that can use the fact that the data is
// sorted.
bool IsSortingOp(FilterOp op) {
  switch (op) {
    case FilterOp::kEq:
    case FilterOp::kLe:
    case FilterOp::kLt:
    case FilterOp::kGe:
    case FilterOp::kGt:
    case FilterOp::kIsNotNull:
    case FilterOp::kIsNull:
      return true;
    case FilterOp::kGlob:
    case FilterOp::kRegex:
    case FilterOp::kNe:
      return false;
  }
  PERFETTO_FATAL("For GCC");
}

void ApplyMinMaxQuery(RowMap& rm,
                      Order o,
                      const column::DataLayerChain& chain) {
  std::vector<uint32_t> table_indices = std::move(rm).TakeAsIndexVector();
  auto indices = Indices::Create(table_indices, Indices::State::kMonotonic);
  std::optional<Token> ret_tok =
      (o.desc) ? chain.MaxElement(indices) : chain.MinElement(indices);
  rm = (ret_tok.has_value()) ? RowMap(std::vector<uint32_t>{ret_tok->payload})
                             : RowMap();
}

void ApplyLimitAndOffset(RowMap& rm, const Query& q) {
  uint32_t end = rm.size();
  uint32_t start = std::min(q.offset, end);
  if (q.limit) {
    end = std::min(end, *q.limit + q.offset);
  }
  rm = rm.SelectRows(RowMap(start, end));
}

}  // namespace

Table::Table(StringPool* pool,
             uint32_t row_count,
             std::vector<ColumnLegacy> columns,
             std::vector<ColumnStorageOverlay> overlays)
    : string_pool_(pool),
      row_count_(row_count),
      overlays_(std::move(overlays)),
      columns_(std::move(columns)) {
  PERFETTO_DCHECK(string_pool_);
}

Table::~Table() = default;

Table& Table::operator=(Table&& other) noexcept {
  row_count_ = other.row_count_;
  string_pool_ = other.string_pool_;

  overlays_ = std::move(other.overlays_);
  columns_ = std::move(other.columns_);
  indexes_ = std::move(other.indexes_);

  storage_layers_ = std::move(other.storage_layers_);
  null_layers_ = std::move(other.null_layers_);
  overlay_layers_ = std::move(other.overlay_layers_);
  chains_ = std::move(other.chains_);

  for (ColumnLegacy& col : columns_) {
    col.table_ = this;
  }
  return *this;
}

Table Table::Copy() const {
  Table table = CopyExceptOverlays();
  for (const ColumnStorageOverlay& overlay : overlays_) {
    table.overlays_.emplace_back(overlay.Copy());
  }
  table.OnConstructionCompleted(storage_layers_, null_layers_, overlay_layers_);
  return table;
}

Table Table::CopyExceptOverlays() const {
  std::vector<ColumnLegacy> cols;
  cols.reserve(columns_.size());
  for (const ColumnLegacy& col : columns_) {
    cols.emplace_back(col, col.index_in_table(), col.overlay_index());
  }
  return {string_pool_, row_count_, std::move(cols), {}};
}

RowMap Table::TryApplyIndex(std::vector<Constraint>& c_vec) const {
  RowMap rm(0, row_count());

  // Prework - use indexes if possible and decide which one.
  std::vector<uint32_t> maybe_idx_cols;
  for (uint32_t i = 0; i < c_vec.size(); i++) {
    const Constraint& c = c_vec[i];
    // Id columns shouldn't use index.
    if (columns()[c.col_idx].IsId()) {
      break;
    }
    // The operation has to support sorting.
    if (!IsSortingOp(c.op)) {
      break;
    }

    maybe_idx_cols.push_back(c.col_idx);

    // For the next col to be able to use index, all previous constraints have
    // to be equality.
    if (c.op != FilterOp::kEq) {
      break;
    }
  }

  OrderedIndices o_idxs;
  while (!maybe_idx_cols.empty()) {
    if (auto maybe_idx = GetIndex(maybe_idx_cols)) {
      o_idxs = std::move(*maybe_idx);
      break;
    }
    maybe_idx_cols.pop_back();
  }

  // If we can't use the index just apply constraints in a standard way.
  if (maybe_idx_cols.empty()) {
    return rm;
  }

  for (uint32_t i = 0; i < maybe_idx_cols.size(); i++) {
    const Constraint& c = c_vec[i];

    Range r =
        ChainForColumn(c.col_idx).OrderedIndexSearch(c.op, c.value, o_idxs);
    o_idxs.data += r.start;
    o_idxs.size = r.size();
  }

  std::vector<uint32_t> res_vec(o_idxs.data, o_idxs.data + o_idxs.size);
  if (res_vec.size() < kIndexVectorThreshold) {
    std::sort(res_vec.begin(), res_vec.end());
    rm = RowMap(std::move(res_vec));
  } else {
    rm = RowMap(BitVector::FromUnsortedIndexVector(std::move(res_vec)));
  }

  c_vec.erase(c_vec.begin(),
              c_vec.begin() + static_cast<uint32_t>(maybe_idx_cols.size()));
  return rm;
}

RowMap Table::QueryToRowMap(const Query& q) const {
  // We need to delay creation of the chains to this point because of Chrome
  // does not want the binary size overhead of including the chain
  // implementations. As they also don't query tables (instead just iterating)
  // over them), using a combination of dead code elimination and linker
  // stripping all chain related code be removed.
  //
  // From rough benchmarking, this has a negligible impact on peformance as this
  // branch is almost never taken.
  if (PERFETTO_UNLIKELY(chains_.size() != columns_.size())) {
    CreateChains();
  }

  auto cs_copy = q.constraints;
  RowMap rm = TryApplyIndex(cs_copy);

  // Filter out constraints that are not using index.
  for (const auto& c : cs_copy) {
    QueryExecutor::ApplyConstraint(c, ChainForColumn(c.col_idx), &rm);
  }

  if (q.order_type != Query::OrderType::kSort) {
    ApplyDistinct(q, &rm);
  }

  // Fastpath for one sort, no distinct and limit 1. This type of query means we
  // need to run Max/Min on orderby column and there is no need for sorting.
  if (q.IsMinMaxQuery()) {
    ApplyMinMaxQuery(rm, q.orders.front(),
                     ChainForColumn(q.orders.front().col_idx));
    return rm;
  }

  if (q.RequireSort()) {
    ApplySort(q, &rm);
  }

  if (q.limit.has_value() || q.offset != 0) {
    ApplyLimitAndOffset(rm, q);
  }

  return rm;
}

Table Table::Sort(const std::vector<Order>& ob) const {
  if (ob.empty()) {
    return Copy();
  }

  // Return a copy of this table with the RowMaps using the computed ordered
  // RowMap.
  Table table = CopyExceptOverlays();
  Query q;
  q.orders = ob;
  RowMap rm = QueryToRowMap(q);
  for (const ColumnStorageOverlay& overlay : overlays_) {
    table.overlays_.emplace_back(overlay.SelectRows(rm));
    PERFETTO_DCHECK(table.overlays_.back().size() == table.row_count());
  }

  // Remove the sorted and row set flags from all the columns.
  for (auto& col : table.columns_) {
    col.flags_ &= ~ColumnLegacy::Flag::kSorted;
    col.flags_ &= ~ColumnLegacy::Flag::kSetId;
  }

  // For the first order by, make the column flag itself as sorted but
  // only if the sort was in ascending order.
  if (!ob.front().desc) {
    table.columns_[ob.front().col_idx].flags_ |= ColumnLegacy::Flag::kSorted;
  }

  std::vector<RefPtr<column::DataLayer>> overlay_layers(table.overlays_.size());
  for (uint32_t i = 0; i < table.overlays_.size(); ++i) {
    if (table.overlays_[i].row_map().IsIndexVector()) {
      overlay_layers[i].reset(new column::ArrangementOverlay(
          table.overlays_[i].row_map().GetIfIndexVector(),
          column::DataLayerChain::Indices::State::kNonmonotonic));
    } else if (table.overlays_[i].row_map().IsBitVector()) {
      overlay_layers[i].reset(new column::SelectorOverlay(
          table.overlays_[i].row_map().GetIfBitVector()));
    } else if (table.overlays_[i].row_map().IsRange()) {
      overlay_layers[i].reset(
          new column::RangeOverlay(table.overlays_[i].row_map().GetIfIRange()));
    }
  }
  table.OnConstructionCompleted(storage_layers_, null_layers_,
                                std::move(overlay_layers));
  return table;
}

void Table::OnConstructionCompleted(
    std::vector<RefPtr<column::DataLayer>> storage_layers,
    std::vector<RefPtr<column::DataLayer>> null_layers,
    std::vector<RefPtr<column::DataLayer>> overlay_layers) {
  for (ColumnLegacy& col : columns_) {
    col.BindToTable(this, string_pool_);
  }
  PERFETTO_CHECK(storage_layers.size() == columns_.size());
  PERFETTO_CHECK(null_layers.size() == columns_.size());
  PERFETTO_CHECK(overlay_layers.size() == overlays_.size());
  storage_layers_ = std::move(storage_layers);
  null_layers_ = std::move(null_layers);
  overlay_layers_ = std::move(overlay_layers);
}

void Table::CreateChains() const {
  chains_.resize(columns_.size());
  for (uint32_t i = 0; i < columns_.size(); ++i) {
    chains_[i] = storage_layers_[i]->MakeChain();
    if (const auto& null_overlay = null_layers_[i]; null_overlay.get()) {
      chains_[i] = null_overlay->MakeChain(std::move(chains_[i]));
    }
    const auto& oly_idx = columns_[i].overlay_index();
    if (const auto& overlay = overlay_layers_[oly_idx]; overlay.get()) {
      chains_[i] = overlay->MakeChain(
          std::move(chains_[i]),
          column::DataLayer::ChainCreationArgs{columns_[i].IsSorted()});
    }
  }
}

void Table::ApplyDistinct(const Query& q, RowMap* rm) const {
  auto& ob = q.orders;
  PERFETTO_DCHECK(!ob.empty());

  // `q.orders` should be treated here only as information on what should we
  // run distinct on, they should not be used for subsequent sorting.
  // TODO(mayzner): Remove the check after we implement the multi column
  // distinct.
  PERFETTO_DCHECK(ob.size() == 1);

  std::vector<uint32_t> table_indices = std::move(*rm).TakeAsIndexVector();
  auto indices = Indices::Create(table_indices, Indices::State::kMonotonic);
  ChainForColumn(ob.front().col_idx).Distinct(indices);
  PERFETTO_DCHECK(indices.tokens.size() <= table_indices.size());

  for (uint32_t i = 0; i < indices.tokens.size(); ++i) {
    table_indices[i] = indices.tokens[i].payload;
  }
  table_indices.resize(indices.tokens.size());

  // Sorting that happens later might require indices to preserve ordering.
  // TODO(mayzner): Needs to be changed after implementing multi column
  // distinct.
  if (q.order_type == Query::OrderType::kDistinctAndSort) {
    std::sort(table_indices.begin(), table_indices.end());
  }

  *rm = RowMap(std::move(table_indices));
}

void Table::ApplySort(const Query& q, RowMap* rm) const {
  const auto& ob = q.orders;
  // Return the RowMap directly if there is a single constraint to sort the
  // table by a column which is already sorted.
  const auto& first_col = columns_[ob.front().col_idx];
  if (ob.size() == 1 && first_col.IsSorted() && !ob.front().desc)
    return;

  // Build an index vector with all the indices for the first |size_| rows.
  std::vector<uint32_t> idx = std::move(*rm).TakeAsIndexVector();
  if (ob.size() == 1 && first_col.IsSorted()) {
    // We special case a single constraint in descending order as this
    // happens any time the |max| function is used in SQLite. We can be
    // more efficient as this column is already sorted so we simply need
    // to reverse the order of this column.
    PERFETTO_DCHECK(ob.front().desc);
    std::reverse(idx.begin(), idx.end());
  } else {
    QueryExecutor::SortLegacy(this, ob, idx);
  }

  *rm = RowMap(std::move(idx));
}

}  // namespace perfetto::trace_processor
