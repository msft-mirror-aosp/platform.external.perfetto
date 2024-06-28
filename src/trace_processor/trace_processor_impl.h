/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_TRACE_PROCESSOR_IMPL_H_
#define SRC_TRACE_PROCESSOR_TRACE_PROCESSOR_IMPL_H_

#include <sqlite3.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "perfetto/base/status.h"
#include "perfetto/trace_processor/basic_types.h"
#include "perfetto/trace_processor/trace_blob_view.h"
#include "perfetto/trace_processor/trace_processor.h"
#include "src/trace_processor/iterator_impl.h"
#include "src/trace_processor/metrics/metrics.h"
#include "src/trace_processor/perfetto_sql/engine/perfetto_sql_engine.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/create_function.h"
#include "src/trace_processor/perfetto_sql/intrinsics/functions/create_view_function.h"
#include "src/trace_processor/trace_processor_storage_impl.h"
#include "src/trace_processor/util/descriptors.h"

namespace perfetto::trace_processor {

// Coordinates the loading of traces from an arbitrary source and allows
// execution of SQL queries on the events in these traces.
class TraceProcessorImpl : public TraceProcessor,
                           public TraceProcessorStorageImpl {
 public:
  explicit TraceProcessorImpl(const Config&);

  TraceProcessorImpl(const TraceProcessorImpl&) = delete;
  TraceProcessorImpl& operator=(const TraceProcessorImpl&) = delete;

  TraceProcessorImpl(TraceProcessorImpl&&) = delete;
  TraceProcessorImpl& operator=(TraceProcessorImpl&&) = delete;

  ~TraceProcessorImpl() override;

  // TraceProcessorStorage implementation:
  base::Status Parse(TraceBlobView) override;
  void Flush() override;
  void NotifyEndOfFile() override;

  // TraceProcessor implementation:
  Iterator ExecuteQuery(const std::string& sql) override;

  base::Status RegisterMetric(const std::string& path,
                              const std::string& sql) override;

  base::Status RegisterSqlModule(SqlModule sql_module) override;

  base::Status ExtendMetricsProto(const uint8_t* data, size_t size) override;

  base::Status ExtendMetricsProto(
      const uint8_t* data,
      size_t size,
      const std::vector<std::string>& skip_prefixes) override;

  base::Status ComputeMetric(const std::vector<std::string>& metric_names,
                             std::vector<uint8_t>* metrics) override;

  base::Status ComputeMetricText(const std::vector<std::string>& metric_names,
                                 TraceProcessor::MetricResultFormat format,
                                 std::string* metrics_string) override;

  std::vector<uint8_t> GetMetricDescriptors() override;

  void InterruptQuery() override;

  size_t RestoreInitialTables() override;

  std::string GetCurrentTraceName() override;
  void SetCurrentTraceName(const std::string&) override;

  void EnableMetatrace(MetatraceConfig config) override;

  base::Status DisableAndReadMetatrace(
      std::vector<uint8_t>* trace_proto) override;

 private:
  // Needed for iterators to be able to access the context.
  friend class IteratorImpl;

  template <typename Table>
  void RegisterStaticTable(Table* table) {
    engine_->RegisterStaticTable(table, Table::Name(),
                                 Table::ComputeStaticSchema());
  }

  bool IsRootMetricField(const std::string& metric_name);

  void InitPerfettoSqlEngine();

  const Config config_;
  std::unique_ptr<PerfettoSqlEngine> engine_;

  DescriptorPool pool_;

  std::vector<metrics::SqlMetricFile> sql_metrics_;
  std::unordered_map<std::string, std::string> proto_field_to_sql_metric_path_;

  // This is atomic because it is set by the CTRL-C signal handler and we need
  // to prevent single-flow compiler optimizations in ExecuteQuery().
  std::atomic<bool> query_interrupted_{false};

  // Track the number of objects registered with SQLite after the constructor.
  uint64_t sqlite_objects_post_constructor_initialization_ = 0;

  std::string current_trace_name_;
  uint64_t bytes_parsed_ = 0;

  // NotifyEndOfFile should only be called once. Set to true whenever it is
  // called.
  bool notify_eof_called_ = false;
};

}  // namespace perfetto::trace_processor

#endif  // SRC_TRACE_PROCESSOR_TRACE_PROCESSOR_IMPL_H_
