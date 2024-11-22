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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PERF_UTIL_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PERF_UTIL_H_

#include <cstddef>
#include <cstdint>

namespace perfetto::trace_processor::perf_importer {

template <typename A, typename B, typename Res>
inline bool SafeAdd(A a, B b, Res* result) {
  return !__builtin_add_overflow(a, b, result);
}

template <typename A, typename B, typename Res>
inline bool SafeMultiply(A a, B b, Res* result) {
  return !__builtin_mul_overflow(a, b, result);
}

}  // namespace perfetto::trace_processor::perf_importer

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PERF_UTIL_H_
