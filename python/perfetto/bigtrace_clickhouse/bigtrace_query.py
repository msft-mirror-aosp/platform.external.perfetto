#!/usr/bin/python3
# Copyright (C) 2024 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Executable script used by Clickhouse to make gRPC calls to the Orchestrator
# from a TVF

import grpc
import sys
import os

from protos.perfetto.bigtrace.orchestrator_pb2 import BigtraceQueryArgs
from protos.perfetto.bigtrace.orchestrator_pb2_grpc import BigtraceOrchestratorStub
from perfetto.common.query_result_iterator import QueryResultIterator

USING_MANUAL_OUTPUT_SCHEMA = True


# TODO(lalitm) Look into using this alongside the generated
# output schema from bigtrace_output_schema.py to improve UX
# when Variants are not experimental and a concept similar
# to macros can be used to simplify boilerplate
def generate_for_udf_output_schema(row):
  # This is required to allow the use of an inner UDF which creates
  # a generic output schema where each column is of the type
  # Tuple(int64_value, string_value, double_value) and values are
  # accessed by type e.g. column_name.string_value
  data = []
  for value in row.__repr__().values():
    data_tuple = "(NULL,NULL,NULL)"
    data_type = type(value).__name__ if type(value) else ""
    data_value = str(value)

    if (data_type == "int"):
      data_tuple = f"({data_value},NULL,NULL)"
    elif (data_type == "str"):
      data_tuple = f"(NULL,'{data_value}',NULL)"
    elif (data_type == "float"):
      data_tuple = f"(NULL,NULL,{data_value})"

    data.append(data_tuple)
  # Convert the list to a tab separated format for Clickhouse to ingest
  data_str = '\t'.join(data)
  print(data_str + '\n', end='')


def generate_for_manual_output_schema(row):
  data = [(str(value) if value is not None else "")
          for value in row.__repr__().values()]
  # Convert the list to a tab separated format for Clickhouse to ingest
  data_str = '\t'.join(data)
  print(data_str + '\n', end='')


def main():
  orchestrator_address = os.environ.get("BIGTRACE_ORCHESTRATOR_ADDRESS")

  for input in sys.stdin:
    # TODO(lalitm) Investigate why this is required for clickhouse
    # to return rows
    input = input.replace('\\n', '')
    # Clickhouse input is specified as tab separated
    traces, sql_query = input.strip("\n").split("\t")
    # Convert the string representation of list of traces given by
    # Clickhouse into a Python list
    trace_list = [x[1:-1] for x in traces[1:-1].split(',')]

    channel = grpc.insecure_channel(orchestrator_address)
    stub = BigtraceOrchestratorStub(channel)
    args = BigtraceQueryArgs(traces=trace_list, sql_query=sql_query)

    responses = stub.Query(args, wait_for_ready=False)
    for response in responses:
      repeated_batches = []
      results = response.result
      column_names = results[0].column_names
      for result in results:
        repeated_batches.extend(result.batch)
      qr_it = QueryResultIterator(column_names, repeated_batches)

      for row in qr_it:
        if USING_MANUAL_OUTPUT_SCHEMA:
          # Output values directly from the row to be typed by
          # the user in a manually entered output schema in
          # the Bigtrace query
          generate_for_manual_output_schema(row)
        else:
          # Convert values to Tuple(int64_value, string_value, double_value)
          # format as automatically generated by bigtrace_output_schema UDF
          generate_for_udf_output_schema(row)

    sys.stdout.flush()


if __name__ == "__main__":
  main()
