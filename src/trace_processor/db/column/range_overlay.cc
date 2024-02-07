/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include "src/trace_processor/db/column/range_overlay.h"

#include "protos/perfetto/trace_processor/serialization.pbzero.h"
#include "src/trace_processor/containers/bit_vector.h"
#include "src/trace_processor/db/column/types.h"
#include "src/trace_processor/tp_metatrace.h"

namespace perfetto::trace_processor::column {

using Range = Range;

RangeOverlay::RangeOverlay(const Range range) : range_(range) {}

std::unique_ptr<DataNode::Queryable> RangeOverlay::MakeQueryable(
    std::unique_ptr<DataNode::Queryable> inner) {
  return std::make_unique<Queryable>(std::move(inner), range_);
}

RangeOverlay::Queryable::Queryable(std::unique_ptr<DataNode::Queryable> inner,
                                   const Range range)
    : inner_(std::move(inner)), range_(range) {
  PERFETTO_CHECK(range.end <= inner_->size());
}

SearchValidationResult RangeOverlay::Queryable::ValidateSearchConstraints(
    SqlValue sql_val,
    FilterOp op) const {
  return inner_->ValidateSearchConstraints(sql_val, op);
}

RangeOrBitVector RangeOverlay::Queryable::Search(FilterOp op,
                                                 SqlValue sql_val,
                                                 Range search_range) const {
  PERFETTO_CHECK(search_range.size() <= range_.size());
  PERFETTO_TP_TRACE(metatrace::Category::DB, "RangeOverlay::Search");

  Range inner_search_range(search_range.start + range_.start,
                           search_range.end + range_.start);
  auto inner_res = inner_->Search(op, sql_val, inner_search_range);
  if (inner_res.IsRange()) {
    Range inner_res_range = std::move(inner_res).TakeIfRange();
    return RangeOrBitVector(Range(inner_res_range.start - range_.start,
                                  inner_res_range.end - range_.start));
  }

  BitVector inner_res_bv = std::move(inner_res).TakeIfBitVector();
  PERFETTO_CHECK(inner_res_bv.size() == inner_search_range.end);
  PERFETTO_CHECK(inner_res_bv.CountSetBits(inner_search_range.start) == 0);

  BitVector::Builder builder(search_range.end, search_range.start);
  uint32_t cur_val = search_range.start;
  uint32_t front_elements = builder.BitsUntilWordBoundaryOrFull();
  for (uint32_t i = 0; i < front_elements; ++i, ++cur_val) {
    builder.Append(inner_res_bv.IsSet(cur_val + range_.start));
  }

  // Fast path: we compare as many groups of 64 elements as we can.
  // This should be very easy for the compiler to auto-vectorize.
  uint32_t fast_path_elements = builder.BitsInCompleteWordsUntilFull();
  for (uint32_t i = 0; i < fast_path_elements; i += BitVector::kBitsInWord) {
    uint64_t word = 0;
    // This part should be optimised by SIMD and is expected to be fast.
    for (uint32_t k = 0; k < BitVector::kBitsInWord; ++k, ++cur_val) {
      bool comp_result = inner_res_bv.IsSet(cur_val + range_.start);
      word |= static_cast<uint64_t>(comp_result) << k;
    }
    builder.AppendWord(word);
  }

  // Slow path: we compare <64 elements and append to fill the Builder.
  uint32_t back_elements = builder.BitsUntilFull();
  for (uint32_t i = 0; i < back_elements; ++i, ++cur_val) {
    builder.Append(inner_res_bv.IsSet(cur_val + range_.start));
  }
  return RangeOrBitVector(std::move(builder).Build());
}

RangeOrBitVector RangeOverlay::Queryable::IndexSearch(FilterOp op,
                                                      SqlValue sql_val,
                                                      Indices indices) const {
  PERFETTO_TP_TRACE(metatrace::Category::DB, "RangeOverlay::IndexSearch");

  std::vector<uint32_t> storage_iv(indices.size);
  // Should be SIMD optimized.
  for (uint32_t i = 0; i < indices.size; ++i) {
    storage_iv[i] = indices.data[i] + range_.start;
  }
  return inner_->IndexSearch(
      op, sql_val, Indices{storage_iv.data(), indices.size, indices.state});
}

Range RangeOverlay::Queryable::OrderedIndexSearch(FilterOp op,
                                                  SqlValue sql_val,
                                                  Indices indices) const {
  PERFETTO_TP_TRACE(metatrace::Category::DB, "RangeOverlay::IndexSearch");

  std::vector<uint32_t> storage_iv(indices.size);
  // Should be SIMD optimized.
  for (uint32_t i = 0; i < indices.size; ++i) {
    storage_iv[i] = indices.data[i] + range_.start;
  }
  return inner_->OrderedIndexSearch(
      op, sql_val, Indices{storage_iv.data(), indices.size, indices.state});
}

void RangeOverlay::Queryable::StableSort(uint32_t*, uint32_t) const {
  // TODO(b/307482437): Implement.
  PERFETTO_FATAL("Not implemented");
}

void RangeOverlay::Queryable::Sort(uint32_t*, uint32_t) const {
  // TODO(b/307482437): Implement.
  PERFETTO_FATAL("Not implemented");
}

void RangeOverlay::Queryable::Serialize(StorageProto*) const {
  PERFETTO_FATAL("Not implemented");
}

}  // namespace perfetto::trace_processor::column