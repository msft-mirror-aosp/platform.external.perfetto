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

#ifndef INCLUDE_PERFETTO_PUBLIC_PROTOS_CONFIG_DATA_SOURCE_CONFIG_PZC_H_
#define INCLUDE_PERFETTO_PUBLIC_PROTOS_CONFIG_DATA_SOURCE_CONFIG_PZC_H_

#include <stdbool.h>
#include <stdint.h>

#include "perfetto/public/pb_macros.h"

PERFETTO_PB_MSG_DECL(perfetto_protos_AndroidGameInterventionListConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_AndroidLogConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_AndroidPolledStateConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_AndroidPowerConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_AndroidSystemPropertyConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_ChromeConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_FtraceConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_GpuCounterConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_HeapprofdConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_InodeFileConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_InterceptorConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_JavaHprofConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_NetworkPacketTraceConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_PackagesListConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_PerfEventConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_ProcessStatsConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_StatsdTracingConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_SysStatsConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_SystemInfoConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_TestConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_TrackEventConfig);
PERFETTO_PB_MSG_DECL(perfetto_protos_VulkanMemoryConfig);

PERFETTO_PB_ENUM_IN_MSG(perfetto_protos_DataSourceConfig, SessionInitiator){
    PERFETTO_PB_ENUM_IN_MSG_ENTRY(perfetto_protos_DataSourceConfig,
                                  SESSION_INITIATOR_UNSPECIFIED) = 0,
    PERFETTO_PB_ENUM_IN_MSG_ENTRY(perfetto_protos_DataSourceConfig,
                                  SESSION_INITIATOR_TRUSTED_SYSTEM) = 1,
};

PERFETTO_PB_MSG(perfetto_protos_DataSourceConfig);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  STRING,
                  const char*,
                  name,
                  1);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  VARINT,
                  uint32_t,
                  target_buffer,
                  2);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  VARINT,
                  uint32_t,
                  trace_duration_ms,
                  3);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  VARINT,
                  bool,
                  prefer_suspend_clock_for_duration,
                  122);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  VARINT,
                  uint32_t,
                  stop_timeout_ms,
                  7);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  VARINT,
                  bool,
                  enable_extra_guardrails,
                  6);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  VARINT,
                  enum perfetto_protos_DataSourceConfig_SessionInitiator,
                  session_initiator,
                  8);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  VARINT,
                  uint64_t,
                  tracing_session_id,
                  4);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_FtraceConfig,
                  ftrace_config,
                  100);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_InodeFileConfig,
                  inode_file_config,
                  102);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_ProcessStatsConfig,
                  process_stats_config,
                  103);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_SysStatsConfig,
                  sys_stats_config,
                  104);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_HeapprofdConfig,
                  heapprofd_config,
                  105);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_JavaHprofConfig,
                  java_hprof_config,
                  110);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_AndroidPowerConfig,
                  android_power_config,
                  106);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_AndroidLogConfig,
                  android_log_config,
                  107);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_GpuCounterConfig,
                  gpu_counter_config,
                  108);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_AndroidGameInterventionListConfig,
                  android_game_intervention_list_config,
                  116);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_PackagesListConfig,
                  packages_list_config,
                  109);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_PerfEventConfig,
                  perf_event_config,
                  111);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_VulkanMemoryConfig,
                  vulkan_memory_config,
                  112);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_TrackEventConfig,
                  track_event_config,
                  113);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_AndroidPolledStateConfig,
                  android_polled_state_config,
                  114);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_AndroidSystemPropertyConfig,
                  android_system_property_config,
                  118);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_StatsdTracingConfig,
                  statsd_tracing_config,
                  117);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_SystemInfoConfig,
                  system_info_config,
                  119);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_ChromeConfig,
                  chrome_config,
                  101);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_InterceptorConfig,
                  interceptor_config,
                  115);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_NetworkPacketTraceConfig,
                  network_packet_trace_config,
                  120);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  STRING,
                  const char*,
                  legacy_config,
                  1000);
PERFETTO_PB_FIELD(perfetto_protos_DataSourceConfig,
                  MSG,
                  perfetto_protos_TestConfig,
                  for_testing,
                  1001);

#endif  // INCLUDE_PERFETTO_PUBLIC_PROTOS_CONFIG_DATA_SOURCE_CONFIG_PZC_H_
