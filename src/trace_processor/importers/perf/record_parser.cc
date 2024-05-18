/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include "src/trace_processor/importers/perf/record_parser.h"

#include <optional>
#include <string>
#include <vector>
#include "perfetto/base/logging.h"
#include "perfetto/base/status.h"
#include "perfetto/ext/base/string_utils.h"
#include "src/trace_processor/importers/common/mapping_tracker.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/perf/perf_data_tracker.h"
#include "src/trace_processor/importers/perf/reader.h"
#include "src/trace_processor/importers/perf/record.h"
#include "src/trace_processor/storage/stats.h"
#include "src/trace_processor/storage/trace_storage.h"
#include "src/trace_processor/tables/profiler_tables_py.h"
#include "src/trace_processor/util/status_macros.h"

namespace perfetto {
namespace trace_processor {
namespace perf_importer {

using FramesTable = tables::StackProfileFrameTable;
using CallsitesTable = tables::StackProfileCallsiteTable;

RecordParser::RecordParser(TraceProcessorContext* context)
    : context_(context), tracker_(PerfDataTracker::GetOrCreate(context_)) {}

RecordParser::~RecordParser() = default;

void RecordParser::ParsePerfRecord(int64_t ts, Record record) {
  if (base::Status status = ParseRecord(ts, std::move(record)); !status.ok()) {
    context_->storage->IncrementStats(stats::perf_record_skipped);
  }
}

base::Status RecordParser::ParseRecord(int64_t ts, Record record) {
  switch (record.header.type) {
    case PERF_RECORD_SAMPLE:
      return ParseSample(ts, std::move(record));

    case PERF_RECORD_MMAP2:
      return ParseMmap2(std::move(record));

    case PERF_RECORD_AUX:
    case PERF_RECORD_AUXTRACE:
    case PERF_RECORD_AUXTRACE_INFO:
      // These should be dealt with at tokenization time
      PERFETTO_CHECK(false);

    default:
      PERFETTO_ELOG("Unknown PERF_RECORD with type %" PRIu32,
                    record.header.type);
  }
  return base::OkStatus();
}

base::Status RecordParser::ParseSample(int64_t ts, Record record) {
  PERFETTO_CHECK(record.attr);

  Reader reader(record.payload.copy());
  ASSIGN_OR_RETURN(PerfDataTracker::PerfSample sample,
                   tracker_->ParseSample(reader, record.attr->sample_type()));

  // The sample has been validated in tokenizer so callchain shouldn't be empty.
  PERFETTO_CHECK(!sample.callchain.empty());

  // First instruction pointer in the callchain should be from kernel space, so
  // it shouldn't be available in mappings.
  UniquePid upid = context_->process_tracker->GetOrCreateProcess(*sample.pid);
  if (context_->mapping_tracker->FindUserMappingForAddress(
          upid, sample.callchain.front())) {
    context_->storage->IncrementStats(stats::perf_samples_skipped);
    return base::ErrStatus(
        "Expected kernel mapping for first instruction pointer, but user space "
        "found.");
  }

  if (sample.callchain.size() == 1) {
    context_->storage->IncrementStats(stats::perf_samples_skipped);
    return base::ErrStatus("Invalid callchain size of 1.");
  }

  std::vector<FramesTable::Row> frame_rows;
  for (uint32_t i = 1; i < sample.callchain.size(); i++) {
    UserMemoryMapping* mapping =
        context_->mapping_tracker->FindUserMappingForAddress(
            upid, sample.callchain[i]);
    if (!mapping) {
      context_->storage->IncrementStats(stats::perf_samples_skipped);
      return base::ErrStatus("Did not find mapping for address %" PRIu64
                             " in process with upid %" PRIu32,
                             sample.callchain[i], upid);
    }
    FramesTable::Row new_row;
    std::string mock_name =
        base::StackString<1024>(
            "%" PRIu64, sample.callchain[i] - mapping->memory_range().start())
            .ToStdString();
    new_row.name = context_->storage->InternString(mock_name.c_str());
    new_row.mapping = mapping->mapping_id();
    new_row.rel_pc =
        static_cast<int64_t>(mapping->ToRelativePc(sample.callchain[i]));
    frame_rows.push_back(new_row);
  }

  // Insert frames. We couldn't do it before as no frames should be added if the
  // mapping couldn't be found for any of them.
  const auto& frames = context_->storage->mutable_stack_profile_frame_table();
  std::vector<FramesTable::Id> frame_ids;
  for (const auto& row : frame_rows) {
    frame_ids.push_back(frames->Insert(row).id);
  }

  // Insert callsites.
  const auto& callsites =
      context_->storage->mutable_stack_profile_callsite_table();

  std::optional<CallsitesTable::Id> parent_callsite_id;
  for (uint32_t i = 0; i < frame_ids.size(); i++) {
    CallsitesTable::Row callsite_row;
    callsite_row.frame_id = frame_ids[i];
    callsite_row.depth = i;
    callsite_row.parent_id = parent_callsite_id;
    parent_callsite_id = callsites->Insert(callsite_row).id;
  }

  // Insert stack sample.
  tables::PerfSampleTable::Row perf_sample_row;
  perf_sample_row.callsite_id = parent_callsite_id;
  perf_sample_row.ts = ts;
  if (sample.cpu) {
    perf_sample_row.cpu = *sample.cpu;
  }
  if (sample.tid) {
    auto utid = context_->process_tracker->GetOrCreateThread(*sample.tid);
    perf_sample_row.utid = utid;
  }
  context_->storage->mutable_perf_sample_table()->Insert(perf_sample_row);

  return base::OkStatus();
}

base::Status RecordParser::ParseMmap2(Record record) {
  Reader reader(record.payload.copy());
  PerfDataTracker::Mmap2Record mmap2;
  reader.Read(mmap2.num);
  std::vector<char> filename_buffer(reader.size_left());
  reader.ReadVector(filename_buffer);
  if (filename_buffer.back() != '\0') {
    return base::ErrStatus(
        "Invalid MMAP2 record: filename is not null terminated.");
  }
  mmap2.filename = std::string(filename_buffer.begin(), filename_buffer.end());
  PERFETTO_CHECK(reader.size_left() == 0);
  mmap2.cpu_mode = record.GetCpuMode();
  tracker_->PushMmap2Record(std::move(mmap2));
  return base::OkStatus();
}

}  // namespace perf_importer
}  // namespace trace_processor
}  // namespace perfetto
