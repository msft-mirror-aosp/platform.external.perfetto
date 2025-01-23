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

#include "src/trace_processor/perfetto_sql/generator/structured_query_generator.h"

#include <cctype>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

#include "perfetto/ext/base/status_or.h"
#include "src/base/test/status_matchers.h"
#include "src/protozero/text_to_proto/text_to_proto.h"
#include "src/trace_processor/perfetto_sql/generator/perfettosql.descriptor.h"
#include "test/gtest_and_gmock.h"

#include "protos/perfetto/perfetto_sql/structured_query.gen.h"

namespace perfetto::trace_processor::perfetto_sql::generator {
namespace {

using testing::UnorderedElementsAre;

using Query = ::perfetto::protos::gen::PerfettoSqlStructuredQuery;

std::vector<uint8_t> ToProto(const std::string& input) {
  base::StatusOr<std::vector<uint8_t>> output = protozero::TextToProto(
      kPerfettosqlDescriptor.data(), kPerfettosqlDescriptor.size(),
      ".perfetto.protos.PerfettoSqlStructuredQuery", "-", input);
  EXPECT_OK(output);
  if (!output.ok()) {
    return {};
  }
  EXPECT_FALSE(output->empty());
  return *output;
}

MATCHER_P(EqualsIgnoringWhitespace, param, "") {
  auto RemoveAllWhitespace = [](const std::string& input) {
    std::string result;
    result.reserve(input.length());
    std::copy_if(input.begin(), input.end(), std::back_inserter(result),
                 [](char c) { return !std::isspace(c); });
    return result;
  };
  return RemoveAllWhitespace(arg) == RemoveAllWhitespace(param);
}

TEST(StructuredQueryGeneratorTest, Smoke) {
  StructuredQueryGenerator gen;
  auto proto = ToProto(R"(
    table: {
      table_name: "memory_rss_and_swap_per_process"
      module_name: "linux.memory.process"
    }
    group_by: {
      column_names: "process_name"
      aggregates: {
        column_name: "rss_and_swap"
        op: DURATION_WEIGHTED_MEAN
        result_column_name: "avg_rss_and_swap"
      }
    }
  )");
  auto ret = gen.Generate(proto.data(), proto.size());
  ASSERT_OK_AND_ASSIGN(std::string res, ret);
  ASSERT_THAT(res, EqualsIgnoringWhitespace(R"(
                WITH sq_0 AS (
                  SELECT
                    process_name AS process_name,
                    SUM(rss_and_swap * dur) / SUM(dur) AS avg_rss_and_swap
                  FROM memory_rss_and_swap_per_process
                  GROUP BY process_name
                )
                SELECT * FROM sq_0
              )"));
  ASSERT_THAT(gen.ComputeReferencedModules(),
              UnorderedElementsAre("linux.memory.process"));
}

TEST(StructuredQueryGeneratorTest, Smoke2) {
  StructuredQueryGenerator gen;
  auto proto = ToProto(R"(
    interval_intersect: {
      base: {
        table: {
          table_name: "thread_slice_cpu_time"
          module_name: "linux.memory.process"
        }
        filters: {
          column_name: "thread_name"
          op: EQUAL
          string_rhs: "bar"
        }
      }
      interval_intersect: {
        simple_slices: {
          slice_name_glob: "baz"
          process_name_glob: "system_server"
        }
      }
    }
    group_by: {
      aggregates: {
        column_name: "cpu_time"
        op: SUM
        result_column_name: "sum_cpu_time"
      }
    }
  )");
  auto ret = gen.Generate(proto.data(), proto.size());
  ASSERT_OK_AND_ASSIGN(std::string res, ret);
  ASSERT_THAT(res.c_str(), EqualsIgnoringWhitespace(R"(
                WITH sq_2 AS (
                  SELECT * FROM (
                    SELECT *
                    FROM _slice_with_thread_and_process_info
                    WHERE name GLOB 'baz'
                      AND process_name GLOB 'system_server'
                  )
                ),
                sq_1 AS (
                  SELECT * FROM thread_slice_cpu_time
                  WHERE thread_name = 'bar'
                ),
                sq_0 AS (
                  SELECT SUM(cpu_time) AS sum_cpu_time
                  FROM (
                    WITH
                      iibase AS (SELECT * FROM sq_1),
                      iisource0 AS (SELECT * FROM sq_2)
                    SELECT ii.ts, ii.dur, iibase.*, iisource0.*
                    FROM _interval_intersect!((iibase, iisource0), ()) ii
                    JOIN iibase ON ii.id_0 = iibase.id
                    JOIN iisource0 ON ii.id_1 = iisource0.id
                  )
                )
                SELECT * FROM sq_0
              )"));
  ASSERT_THAT(gen.ComputeReferencedModules(),
              UnorderedElementsAre("intervals.intersect",
                                   "linux.memory.process", "slices.slices"));
}

TEST(StructuredQueryGeneratorTest, ColumnSelection) {
  StructuredQueryGenerator gen;
  auto proto = ToProto(R"(
    id: "table_source_thread_slice"
    table: {
      table_name: "thread_slice"
      module_name: "slices.with_context"
      column_names: "id"
      column_names: "ts"
      column_names: "dur"
    }
    select_columns: {column_name: "id"}
    select_columns: {
      column_name: "dur"
      alias: "cheese"
    }
    select_columns: {column_name: "ts"}
  )");
  auto ret = gen.Generate(proto.data(), proto.size());
  ASSERT_OK_AND_ASSIGN(std::string res, ret);
  ASSERT_THAT(res.c_str(), EqualsIgnoringWhitespace(R"(
    WITH sq_table_source_thread_slice AS
      (SELECT
        id,
        dur AS cheese,
        ts
      FROM thread_slice)
    SELECT * FROM sq_table_source_thread_slice
  )"));
}

}  // namespace
}  // namespace perfetto::trace_processor::perfetto_sql::generator
