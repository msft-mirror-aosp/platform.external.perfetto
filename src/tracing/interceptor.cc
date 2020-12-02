/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include "perfetto/tracing/interceptor.h"

#include "perfetto/tracing/internal/tracing_muxer.h"

namespace perfetto {

InterceptorBase::~InterceptorBase() = default;
InterceptorBase::ThreadLocalState::~ThreadLocalState() = default;

// static
void InterceptorBase::RegisterImpl(
    const InterceptorDescriptor& descriptor,
    std::function<std::unique_ptr<InterceptorBase>()> factory,
    InterceptorBase::TLSFactory tls_factory,
    InterceptorBase::TracePacketCallback on_trace_packet) {
  auto* tracing_impl = internal::TracingMuxer::Get();
  PERFETTO_DCHECK(tracing_impl);
  if (!tracing_impl) {
    PERFETTO_ELOG(
        "Call Tracing::Initialize() before registering interceptors.");
    return;
  }
  tracing_impl->RegisterInterceptor(descriptor, factory, tls_factory,
                                    on_trace_packet);
}

}  // namespace perfetto
