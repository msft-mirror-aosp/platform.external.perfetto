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

#include <grpcpp/grpcpp.h>
#include <grpcpp/support/status.h>
#include <cstdint>
#include <memory>

#include "perfetto/base/status.h"
#include "perfetto/ext/trace_processor/rpc/query_result_serializer.h"
#include "perfetto/trace_processor/read_trace.h"
#include "perfetto/trace_processor/trace_processor.h"
#include "protos/perfetto/bigtrace/worker.grpc.pb.h"
#include "protos/perfetto/bigtrace/worker.pb.h"
#include "src/bigtrace/worker/worker_impl.h"

#include "perfetto/ext/base/getopt.h"

namespace perfetto {
namespace bigtrace {
namespace {

struct CommandLineOptions {
  std::string port;
};

CommandLineOptions ParseCommandLineOptions(int argc, char** argv) {
  CommandLineOptions command_line_options;
  static option long_options[] = {{"port", required_argument, nullptr, 'p'},
                                  {nullptr, 0, nullptr, 0}};
  int c;
  while ((c = getopt_long(argc, argv, "w:", long_options, nullptr)) != -1) {
    switch (c) {
      case 'p':
        command_line_options.port = optarg;
        break;
      default:
        PERFETTO_ELOG("Usage: %s --port=port", argv[0]);
        break;
    }
  }
  return command_line_options;
}

base::Status WorkerMain(int argc, char** argv) {
  // Setup the Worker Server
  CommandLineOptions options = ParseCommandLineOptions(argc, argv);
  std::string port = !options.port.empty() ? options.port : "5052";

  std::string server_address("localhost:" + port);
  auto service = std::make_unique<WorkerImpl>();
  grpc::ServerBuilder builder;
  builder.RegisterService(service.get());
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  PERFETTO_LOG("Worker server listening on %s", server_address.c_str());

  server->Wait();

  return base::OkStatus();
}

}  // namespace
}  // namespace bigtrace
}  // namespace perfetto

int main(int argc, char** argv) {
  auto status = perfetto::bigtrace::WorkerMain(argc, argv);
  if (!status.ok()) {
    fprintf(stderr, "%s\n", status.c_message());
    return 1;
  }
  return 0;
}
