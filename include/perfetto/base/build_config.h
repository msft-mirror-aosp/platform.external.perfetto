/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef INCLUDE_PERFETTO_BASE_BUILD_CONFIG_H_
#define INCLUDE_PERFETTO_BASE_BUILD_CONFIG_H_

// Allows to define build flags that give a compiler error if the header that
// defined the flag is not included, instead of silently ignoring the #if block.
#define PERFETTO_BUILDFLAG_CAT_INDIRECT(a, b) a##b
#define PERFETTO_BUILDFLAG_CAT(a, b) PERFETTO_BUILDFLAG_CAT_INDIRECT(a, b)
#define PERFETTO_BUILDFLAG(flag) \
  (PERFETTO_BUILDFLAG_CAT(PERFETTO_BUILDFLAG_DEFINE_, flag)())

#if defined(__ANDROID__)
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_ANDROID() 1
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_LINUX() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_WIN() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_APPLE() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_MAC() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_IOS() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_WASM() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_FUCHSIA() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_NACL() 0
#elif defined(__APPLE__)
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_ANDROID() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_APPLE() 1
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_LINUX() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_WIN() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_WASM() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_FUCHSIA() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_NACL() 0
// Include TARGET_OS_IPHONE when on __APPLE__ systems.
#include <TargetConditionals.h>
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_MAC() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_IOS() 1
#else
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_MAC() 1
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_IOS() 0
#endif
#elif defined(__linux__)
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_ANDROID() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_LINUX() 1
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_WIN() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_APPLE() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_MAC() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_IOS() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_WASM() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_FUCHSIA() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_NACL() 0
#elif defined(_WIN32)
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_ANDROID() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_LINUX() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_WIN() 1
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_APPLE() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_MAC() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_IOS() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_WASM() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_FUCHSIA() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_NACL() 0
#elif defined(__EMSCRIPTEN__)
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_ANDROID() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_LINUX() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_WIN() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_APPLE() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_MAC() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_IOS() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_WASM() 1
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_FUCHSIA() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_NACL() 0
#elif defined(__Fuchsia__)
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_ANDROID() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_APPLE() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_MAC() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_IOS() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_LINUX() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_WIN() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_WASM() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_FUCHSIA() 1
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_NACL() 0
#elif defined(__native_client__)
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_ANDROID() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_LINUX() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_WIN() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_APPLE() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_MAC() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_IOS() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_WASM() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_FUCHSIA() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_OS_NACL() 1
#else
#error OS not supported (see build_config.h)
#endif

#if defined(__clang__)
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_COMPILER_CLANG() 1
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_COMPILER_GCC() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_COMPILER_MSVC() 0
#elif defined(__GNUC__) // Careful: Clang also defines this!
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_COMPILER_CLANG() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_COMPILER_GCC() 1
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_COMPILER_MSVC() 0
#elif defined(_MSC_VER)
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_COMPILER_CLANG() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_COMPILER_GCC() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_COMPILER_MSVC() 1
#else
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_COMPILER_CLANG() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_COMPILER_GCC() 0
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_COMPILER_MSVC() 0
#endif

#if defined(PERFETTO_BUILD_WITH_ANDROID_USERDEBUG)
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_ANDROID_USERDEBUG_BUILD() 1
#else
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_ANDROID_USERDEBUG_BUILD() 0
#endif

// Processor architecture detection.  For more info on what's defined, see:
//   http://msdn.microsoft.com/en-us/library/b0084kay.aspx
//   http://www.agner.org/optimize/calling_conventions.pdf
//   or with gcc, run: "echo | gcc -E -dM -"
#if defined(__aarch64__) || defined(_M_ARM64)
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_ARCH_CPU_ARM64() 1
#else
#define PERFETTO_BUILDFLAG_DEFINE_PERFETTO_ARCH_CPU_ARM64() 0
#endif

// perfetto_build_flags.h contains the tweakable build flags defined via GN.
// - In GN builds (e.g., standalone, chromium, v8) this file is generated at
//   build time via the gen_rule //gn/gen_buildflags.
// - In Android in-tree builds, this file is generated by tools/gen_android_bp
//   and checked in into include/perfetto/base/build_configs/android_tree/. The
//   default cflags add this path to the default include path.
// - Similarly, in bazel builds, this file is generated by tools/gen_bazel and
//   checked in into include/perfetto/base/build_configs/bazel/.
// - In amalgamated builds, this file is generated by tools/gen_amalgamated and
//   added to the amalgamated headers.
#include "perfetto_build_flags.h"  // no-include-violation-check

#endif  // INCLUDE_PERFETTO_BASE_BUILD_CONFIG_H_
