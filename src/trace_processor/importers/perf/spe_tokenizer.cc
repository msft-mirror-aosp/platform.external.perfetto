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

#include "src/trace_processor/importers/perf/spe_tokenizer.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <utility>

#include "perfetto/base/logging.h"
#include "perfetto/base/status.h"
#include "perfetto/ext/base/status_or.h"
#include "perfetto/trace_processor/trace_blob_view.h"
#include "src/trace_processor/importers/common/clock_tracker.h"
#include "src/trace_processor/importers/perf/aux_data_tokenizer.h"
#include "src/trace_processor/importers/perf/aux_record.h"
#include "src/trace_processor/importers/perf/itrace_start_record.h"
#include "src/trace_processor/importers/perf/spe.h"
#include "src/trace_processor/sorter/trace_sorter.h"
#include "src/trace_processor/storage/stats.h"
#include "src/trace_processor/types/trace_processor_context.h"

namespace perfetto::trace_processor::perf_importer {

void SpeTokenizer::OnDataLoss(uint64_t) {
  // Clear any inflight parsing.
  buffer_.PopFrontUntil(buffer_.end_offset());
}

base::Status SpeTokenizer::OnItraceStartRecord(ItraceStartRecord) {
  // Clear any inflight parsing.
  buffer_.PopFrontUntil(buffer_.end_offset());
  return base::OkStatus();
}

base::Status SpeTokenizer::Parse(AuxRecord aux, TraceBlobView data) {
  last_aux_record_ = std::move(aux);
  buffer_.PushBack(std::move(data));
  while (ProcessRecord()) {
  }
  return base::OkStatus();
}

bool SpeTokenizer::ProcessRecord() {
  for (auto it = buffer_.GetIterator(); it;) {
    uint8_t byte_0 = *it;
    // Must be true (we passed the for loop condition).
    it.MaybeAdvance(1);

    if (spe::IsExtendedHeader(byte_0)) {
      if (!it) {
        return false;
      }
      uint8_t byte_1 = *it;
      uint8_t payload_size =
          spe::ExtendedHeader(byte_0, byte_1).GetPayloadSize();
      if (!it.MaybeAdvance(payload_size + 1)) {
        return false;
      }
      continue;
    }

    spe::ShortHeader short_header(byte_0);
    uint8_t payload_size = short_header.GetPayloadSize();
    if (!it.MaybeAdvance(payload_size)) {
      return false;
    }

    if (short_header.IsEndPacket()) {
      size_t record_len = it.file_offset() - buffer_.start_offset();
      TraceBlobView record =
          *buffer_.SliceOff(buffer_.start_offset(), record_len);
      buffer_.PopFrontUntil(it.file_offset());
      Emit(std::move(record), std::nullopt);
      return true;
    }

    if (short_header.IsTimestampPacket()) {
      size_t record_len = it.file_offset() - buffer_.start_offset();
      TraceBlobView record =
          *buffer_.SliceOff(buffer_.start_offset(), record_len);
      buffer_.PopFrontUntil(it.file_offset());
      Emit(std::move(record), ReadTimestamp(record));
      return true;
    }
  }
  return false;
}

uint64_t SpeTokenizer::ReadTimestamp(const TraceBlobView& record) {
  PERFETTO_CHECK(record.size() >= 8);
  uint64_t timestamp;
  memcpy(&timestamp, record.data() + record.size() - 8, 8);
  return timestamp;
}

base::Status SpeTokenizer::NotifyEndOfStream() {
  return base::OkStatus();
}

void SpeTokenizer::Emit(TraceBlobView record, std::optional<uint64_t> cycles) {
  PERFETTO_CHECK(last_aux_record_);

  std::optional<uint64_t> perf_time;

  if (cycles.has_value()) {
    perf_time = stream_.ConvertTscToPerfTime(*cycles);
  } else {
    context_->storage->IncrementStats(stats::spe_no_timestamp);
  }

  if (!perf_time && last_aux_record_->sample_id.has_value()) {
    perf_time = last_aux_record_->sample_id->time();
  }

  if (!perf_time) {
    context_->sorter->PushSpeRecord(context_->sorter->max_timestamp(),
                                    std::move(record));
    return;
  }

  base::StatusOr<int64_t> trace_time = context_->clock_tracker->ToTraceTime(
      last_aux_record_->attr->clock_id(), static_cast<int64_t>(*perf_time));
  if (!trace_time.ok()) {
    context_->storage->IncrementStats(stats::spe_record_droped);
    return;
  }
  context_->sorter->PushSpeRecord(*trace_time, std::move(record));
}

}  // namespace perfetto::trace_processor::perf_importer
