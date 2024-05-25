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

#ifndef SRC_TRACE_PROCESSOR_FORWARDING_TRACE_PARSER_H_
#define SRC_TRACE_PROCESSOR_FORWARDING_TRACE_PARSER_H_

#include "perfetto/base/status.h"
#include "perfetto/trace_processor/trace_blob_view.h"
#include "src/trace_processor/importers/common/chunked_trace_reader.h"
#include "src/trace_processor/util/trace_type.h"

namespace perfetto {
namespace trace_processor {

class TraceProcessorContext;

class ForwardingTraceParser : public ChunkedTraceReader {
 public:
  explicit ForwardingTraceParser(TraceProcessorContext*);
  ~ForwardingTraceParser() override;

  // ChunkedTraceReader implementation
  util::Status Parse(TraceBlobView) override;
  void NotifyEndOfFile() override;

 private:
  base::Status Init(const TraceBlobView&);
  void UpdateSorterForTraceType(TraceType trace_type);
  TraceProcessorContext* const context_;
  std::unique_ptr<ChunkedTraceReader> reader_;
};

}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_FORWARDING_TRACE_PARSER_H_
