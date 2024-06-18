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

// Autogenerated by the ProtoZero C compiler plugin.
// Invoked by tools/gen_c_protos
// DO NOT EDIT.
#ifndef INCLUDE_PERFETTO_PUBLIC_PROTOS_COMMON_BUILTIN_CLOCK_PZC_H_
#define INCLUDE_PERFETTO_PUBLIC_PROTOS_COMMON_BUILTIN_CLOCK_PZC_H_

#include <stdbool.h>
#include <stdint.h>

#include "perfetto/public/pb_macros.h"

PERFETTO_PB_ENUM(perfetto_protos_BuiltinClock){
    PERFETTO_PB_ENUM_ENTRY(perfetto_protos_BUILTIN_CLOCK_UNKNOWN) = 0,
    PERFETTO_PB_ENUM_ENTRY(perfetto_protos_BUILTIN_CLOCK_REALTIME) = 1,
    PERFETTO_PB_ENUM_ENTRY(perfetto_protos_BUILTIN_CLOCK_REALTIME_COARSE) = 2,
    PERFETTO_PB_ENUM_ENTRY(perfetto_protos_BUILTIN_CLOCK_MONOTONIC) = 3,
    PERFETTO_PB_ENUM_ENTRY(perfetto_protos_BUILTIN_CLOCK_MONOTONIC_COARSE) = 4,
    PERFETTO_PB_ENUM_ENTRY(perfetto_protos_BUILTIN_CLOCK_MONOTONIC_RAW) = 5,
    PERFETTO_PB_ENUM_ENTRY(perfetto_protos_BUILTIN_CLOCK_BOOTTIME) = 6,
    PERFETTO_PB_ENUM_ENTRY(perfetto_protos_BUILTIN_CLOCK_TSC) = 9,
    PERFETTO_PB_ENUM_ENTRY(perfetto_protos_BUILTIN_CLOCK_MAX_ID) = 63,
};

#endif  // INCLUDE_PERFETTO_PUBLIC_PROTOS_COMMON_BUILTIN_CLOCK_PZC_H_
