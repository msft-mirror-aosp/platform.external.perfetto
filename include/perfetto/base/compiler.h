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

#ifndef INCLUDE_PERFETTO_BASE_COMPILER_H_
#define INCLUDE_PERFETTO_BASE_COMPILER_H_

#include <stddef.h>
#include <type_traits>

#include "perfetto/base/build_config.h"

#define PERFETTO_LIKELY(_x) __builtin_expect(!!(_x), 1)
#define PERFETTO_UNLIKELY(_x) __builtin_expect(!!(_x), 0)

#if defined(__GNUC__) || defined(__clang__)
#define PERFETTO_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define PERFETTO_WARN_UNUSED_RESULT
#endif

#if defined(__clang__)
#define PERFETTO_ALWAYS_INLINE __attribute__((__always_inline__))
#define PERFETTO_NO_INLINE __attribute__((__noinline__))
#else
// GCC is too pedantic and often fails with the error:
// "always_inline function might not be inlinable"
#define PERFETTO_ALWAYS_INLINE
#define PERFETTO_NO_INLINE
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PERFETTO_DEBUG_FUNCTION_IDENTIFIER() __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define PERFETTO_DEBUG_FUNCTION_IDENTIFIER() __FUNCSIG__
#else
#define PERFETTO_DEBUG_FUNCTION_IDENTIFIER() \
  static_assert(false, "Not implemented for this compiler")
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PERFETTO_PRINTF_FORMAT(x, y) \
  __attribute__((__format__(__printf__, x, y)))
#else
#define PERFETTO_PRINTF_FORMAT(x, y)
#endif

#if PERFETTO_BUILDFLAG(PERFETTO_OS_IOS)
// TODO(b/158814068): For iOS builds, thread_local is only supported since iOS
// 8. We'd have to use pthread for thread local data instead here. For now, just
// define it to nothing since we don't support running perfetto or the client
// lib on iOS right now.
#define PERFETTO_THREAD_LOCAL
#else
#define PERFETTO_THREAD_LOCAL thread_local
#endif

#if defined(__clang__)
#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
extern "C" void __asan_poison_memory_region(void const volatile*, size_t);
extern "C" void __asan_unpoison_memory_region(void const volatile*, size_t);
#define PERFETTO_ASAN_POISON(a, s) __asan_poison_memory_region((a), (s))
#define PERFETTO_ASAN_UNPOISON(a, s) __asan_unpoison_memory_region((a), (s))
#else
#define PERFETTO_ASAN_POISON(addr, size)
#define PERFETTO_ASAN_UNPOISON(addr, size)
#endif  // __has_feature(address_sanitizer)
#else
#define PERFETTO_ASAN_POISON(addr, size)
#define PERFETTO_ASAN_UNPOISON(addr, size)
#endif  // __clang__

namespace perfetto {
namespace base {

template <typename... T>
inline void ignore_result(const T&...) {}

}  // namespace base
}  // namespace perfetto

#endif  // INCLUDE_PERFETTO_BASE_COMPILER_H_
