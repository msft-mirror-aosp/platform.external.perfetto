--
-- Copyright 2024 The Android Open Source Project
--
-- Licensed under the Apache License, Version 2.0 (the 'License');
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     https://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an 'AS IS' BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.

INCLUDE PERFETTO MODULE android.oom_adjuster;
INCLUDE PERFETTO MODULE memory.linux.general;

-- All memory counters tables.

CREATE PERFETTO VIEW _anon_rss AS
SELECT
  ts,
  dur,
  upid,
  value AS anon_rss_val
FROM _all_counters_per_process
WHERE name = 'mem.rss.anon';

CREATE PERFETTO VIEW _file_rss AS
SELECT
  ts,
  dur,
  upid,
  value AS file_rss_val
FROM _all_counters_per_process
WHERE name = 'mem.rss.file';

CREATE PERFETTO VIEW _shmem_rss AS
SELECT
  ts,
  dur,
  upid,
  value AS shmem_rss_val
FROM _all_counters_per_process
WHERE name = 'mem.rss.shmem';

CREATE PERFETTO VIEW _swap AS
SELECT
  ts,
  dur,
  upid,
  value AS swap_val
FROM _all_counters_per_process
WHERE name = 'mem.swap';

-- Span joins

CREATE VIRTUAL TABLE _anon_swap_sj
USING SPAN_OUTER_JOIN(
  _anon_rss PARTITIONED upid,
  _swap PARTITIONED upid);

CREATE VIRTUAL TABLE _anon_swap_file_sj
USING SPAN_OUTER_JOIN(
  _anon_swap_sj PARTITIONED upid,
  _file_rss PARTITIONED upid
);

CREATE VIRTUAL TABLE _rss_swap_sj
USING SPAN_OUTER_JOIN(
  _anon_swap_file_sj PARTITIONED upid,
  _shmem_rss PARTITIONED upid
);

CREATE PERFETTO TABLE _memory_rss_and_swap_per_process_table AS
SELECT
  ts, dur, upid,
  cast_int!(anon_rss_val) AS anon_rss,
  cast_int!(file_rss_val) AS file_rss,
  cast_int!(shmem_rss_val) AS shmem_rss,
  cast_int!(swap_val) AS swap
FROM _rss_swap_sj;


-- Memory metrics timeline for each process.
CREATE PERFETTO VIEW memory_rss_and_swap_per_process(
  -- Timestamp
  ts INT,
  -- Duration
  dur INT,
  -- Upid of the process
  upid INT,
  -- Pid of the process
  pid INT,
  -- Name of the process
  process_name STRING,
  -- Anon RSS counter value
  anon_rss INT,
  -- File RSS counter value
  file_rss INT,
  -- Shared memory RSS counter value
  shmem_rss INT,
  -- Total RSS value. Sum of `anon_rss`, `file_rss` and `shmem_rss`. Returns
  -- value even if one of the values is NULL.
  rss INT,
  -- Swap counter value
  swap INT,
  -- Sum or `anon_rss` and `swap`. Returns value even if one of the values is
  -- NULL.
  anon_rss_and_swap INT,
  -- Sum or `rss` and `swap`. Returns value even if one of the values is NULL.
  rss_and_swap INT
) AS
SELECT
  ts,
  dur,
  upid,
  pid,
  name AS process_name,
  anon_rss,
  file_rss,
  shmem_rss,
  -- We do COALESCE only on `shmem_rss` and `swap`, as it can be expected all
  -- process start to emit anon rss and file rss events (you'll need to at
  -- least read code and have some memory to work with) - so the NULLs are real
  --  values. But it is possible that you will never swap or never use shmem,
  -- so those values are expected to often be NULLs, which shouldn't propagate
  -- into the values like `anon_and_swap` or `rss`.
  file_rss + anon_rss + COALESCE(shmem_rss, 0) AS rss,
  swap,
  anon_rss + COALESCE(swap, 0) AS anon_rss_and_swap,
  anon_rss + file_rss  + COALESCE(shmem_rss, 0) + COALESCE(swap, 0) AS rss_and_swap
FROM _memory_rss_and_swap_per_process_table
JOIN process USING (upid);

-- OOM score tables

CREATE VIRTUAL TABLE _mem_ooms_sj
USING SPAN_OUTER_JOIN(
  android_oom_adj_intervals PARTITIONED upid,
  _memory_rss_and_swap_per_process_table PARTITIONED upid);

-- Process memory and it's OOM adjuster scores. Detects transitions, each new
-- interval means that either the memory or OOM adjuster score of the process changed.
CREATE PERFETTO TABLE memory_oom_score_with_rss_and_swap_per_process(
  -- Timestamp the oom_adj score or memory of the process changed
  ts INT,
  -- Duration until the next oom_adj score or memory change of the process.
  dur INT,
  -- oom adjuster score of the process.
  score INT,
  -- oom adjuster bucket of the process.
  bucket STRING,
  -- Upid of the process having an oom_adj update.
  upid INT,
  -- Name of the process having an oom_adj update.
  process_name STRING,
  -- Pid of the process having an oom_adj update.
  pid INT,
  -- Slice of the latest oom_adj update in the system_server. Alias of
  -- `slice.id`.
  oom_adj_id INT,
  -- Timestamp of the latest oom_adj update in the system_server.
  oom_adj_ts INT,
  -- Duration of the latest oom_adj update in the system_server.
  oom_adj_dur INT,
  -- Track of the latest oom_adj update in the system_server. Alias of
  -- `track.id`.
  oom_adj_track_id INT,
  -- Thread name of the latest oom_adj update in the system_server.
  oom_adj_thread_name STRING,
  -- Reason for the latest oom_adj update in the system_server.
  oom_adj_reason STRING,
  -- Trigger for the latest oom_adj update in the system_server.
  oom_adj_trigger STRING,
  -- Anon RSS counter value
  anon_rss INT,
  -- File RSS counter value
  file_rss INT,
  -- Shared memory RSS counter value
  shmem_rss INT,
  -- Total RSS value. Sum of `anon_rss`, `file_rss` and `shmem_rss`. Returns
  -- value even if one of the values is NULL.
  rss INT,
  -- Swap counter value
  swap INT,
  -- Sum or `anon_rss` and `swap`. Returns value even if one of the values is
  -- NULL.
  anon_rss_and_swap INT,
  -- Sum or `rss` and `swap`. Returns value even if one of the values is NULL.
  rss_and_swap INT
) AS
SELECT
  ts,
  dur,
  score,
  bucket,
  upid,
  process_name,
  pid,
  oom_adj_id,
  oom_adj_ts,
  oom_adj_dur,
  oom_adj_track_id,
  oom_adj_thread_name,
  oom_adj_reason,
  oom_adj_trigger,
  anon_rss,
  file_rss,
  shmem_rss,
  file_rss + anon_rss + COALESCE(shmem_rss, 0) AS rss,
  swap,
  anon_rss + COALESCE(swap, 0) AS anon_rss_and_swap,
  anon_rss + file_rss  + COALESCE(shmem_rss, 0) + COALESCE(swap, 0) AS rss_and_swap
FROM _mem_ooms_sj
JOIN process USING (upid);
