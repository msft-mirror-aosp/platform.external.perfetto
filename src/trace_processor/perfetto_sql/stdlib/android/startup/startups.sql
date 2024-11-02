--
-- Copyright 2019 The Android Open Source Project
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     https://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.

INCLUDE PERFETTO MODULE android.process_metadata;
INCLUDE PERFETTO MODULE android.startup.startups_maxsdk28;
INCLUDE PERFETTO MODULE android.startup.startups_minsdk29;
INCLUDE PERFETTO MODULE android.startup.startups_minsdk33;
INCLUDE PERFETTO MODULE android.version;

-- All activity startups in the trace by startup id.
-- Populated by different scripts depending on the platform version/contents.
CREATE PERFETTO TABLE android_startups(
  -- Startup id.
  startup_id INT,
  -- Timestamp of startup start.
  ts INT,
  -- Timestamp of startup end.
  ts_end INT,
  -- Startup duration.
  dur INT,
  -- Package name.
  package STRING,
  -- Startup type.
  startup_type STRING
)
AS
WITH version AS (
  SELECT CASE
    WHEN _android_sdk_version() >= 33 THEN 33
    WHEN _android_sdk_version() >= 29 THEN 29
    WHEN _android_sdk_version() IS NOT NULL THEN 28
    WHEN (
      SELECT COUNT()
      FROM slice
      WHERE name GLOB 'launchingActivity#*:*'
    ) > 0 THEN 33
    WHEN (
      SELECT COUNT()
      FROM slice
      WHERE name GLOB 'MetricsLogger:*'
    ) > 0 THEN 29
    ELSE 28
  END AS v
)
SELECT _auto_id as startup_id, ts, ts_end, dur, package, startup_type
FROM _startups_maxsdk28
WHERE (SELECT v from version) = 28
UNION ALL
SELECT _auto_id as startup_id, ts, ts_end, dur, package, startup_type
FROM _startups_minsdk29
WHERE (SELECT v from version) = 29
UNION ALL
SELECT startup_id, ts, ts_end, dur, package, startup_type
FROM _startups_minsdk33
WHERE (SELECT v from version) = 33;

-- Create a table containing only the slices which are necessary for determining
-- whether a startup happened.
CREATE PERFETTO TABLE _startup_indicator_slices AS
SELECT ts, name, track_id
FROM slice
WHERE name IN ('bindApplication', 'activityStart', 'activityResume');

CREATE PERFETTO FUNCTION _startup_indicator_slice_count(start_ts LONG,
                                                                end_ts LONG,
                                                                utid INT,
                                                                name STRING)
RETURNS INT AS
SELECT COUNT(1)
FROM thread_track t
JOIN _startup_indicator_slices s ON s.track_id = t.id
WHERE
  t.utid = $utid AND
  s.ts >= $start_ts AND
  s.ts < $end_ts AND
  s.name = $name;

-- Maps a startup to the set of processes that handled the activity start.
--
-- The vast majority of cases should be a single process. However it is
-- possible that the process dies during the activity startup and is respawned.
CREATE PERFETTO TABLE android_startup_processes(
  -- Startup id.
  startup_id INT,
  -- Upid of process on which activity started.
  upid INT,
  -- Pid of process on which activity started.
  pid INT,
  -- Type of the startup.
  startup_type STRING
) AS
-- This is intentionally a materialized query. For some reason, if we don't
-- materialize, we end up with a query which is an order of magnitude slower :(
WITH startup_with_type AS MATERIALIZED (
  SELECT
    startup_id,
    upid,
    pid,
    CASE
      -- type parsed from platform event takes precedence if available
      WHEN startup_type IS NOT NULL THEN startup_type
      WHEN bind_app > 0 AND a_start > 0 AND a_resume > 0 THEN 'cold'
      WHEN a_start > 0 AND a_resume > 0 THEN 'warm'
      WHEN a_resume > 0 THEN 'hot'
      ELSE NULL
    END AS startup_type
  FROM (
    SELECT
      l.startup_id,
      l.startup_type,
      p.upid,
      p.pid,
      _startup_indicator_slice_count(l.ts, l.ts_end, t.utid, 'bindApplication') AS bind_app,
      _startup_indicator_slice_count(l.ts, l.ts_end, t.utid, 'activityStart') AS a_start,
      _startup_indicator_slice_count(l.ts, l.ts_end, t.utid, 'activityResume') AS a_resume
    FROM android_startups l
    JOIN android_process_metadata p ON (
      l.package = p.package_name
      -- If the package list data source was not enabled in the trace, nothing
      -- will match the above constraint so also match any process whose name
      -- is a prefix of the package name.
      OR (
        (SELECT COUNT(1) = 0 FROM package_list)
        AND p.process_name GLOB l.package || '*'
      )
      )
    JOIN thread t ON (p.upid = t.upid AND t.is_main_thread)
    -- Filter out the non-startup processes with the same package name as that of a startup.
    WHERE a_resume > 0
  )
)
SELECT *
FROM startup_with_type
WHERE startup_type IS NOT NULL;


-- Maps a startup to the set of threads on processes that handled the
-- activity start.
CREATE PERFETTO VIEW android_startup_threads(
  -- Startup id.
  startup_id INT,
  -- Timestamp of start.
  ts INT,
  -- Duration of startup.
  dur INT,
  -- Upid of process involved in startup.
  upid INT,
  -- Pid if process involved in startup.
  pid INT,
  -- Utid of the thread.
  utid INT,
  -- Tid of the thread.
  tid INT,
  -- Name of the thread.
  thread_name STRING,
  -- Thread is a main thread.
  is_main_thread BOOL
) AS
SELECT
  startups.startup_id,
  startups.ts,
  startups.dur,
  android_startup_processes.upid,
  android_startup_processes.pid,
  thread.utid,
  thread.tid,
  thread.name AS thread_name,
  thread.is_main_thread AS is_main_thread
FROM android_startups startups
JOIN android_startup_processes USING (startup_id)
JOIN thread USING (upid);

---
--- Functions
---

-- All the slices for all startups in trace.
--
-- Generally, this view should not be used. Instead, use one of the view functions related
-- to the startup slices which are created from this table.
CREATE PERFETTO VIEW android_thread_slices_for_all_startups(
  -- Timestamp of startup.
  startup_ts INT,
  -- Timestamp of startup end.
  startup_ts_end INT,
  -- Startup id.
  startup_id INT,
  -- UTID of thread with slice.
  utid INT,
  --Tid of thread.
  tid INT,
  -- Name of thread.
  thread_name STRING,
  -- Whether it is main thread.
  is_main_thread BOOL,
  -- Arg set id.
  arg_set_id INT,
  -- Slice id.
  slice_id INT,
  -- Name of slice.
  slice_name STRING,
  -- Timestamp of slice start.
  slice_ts INT,
  -- Slice duration.
  slice_dur INT
) AS
SELECT
  st.ts AS startup_ts,
  st.ts + st.dur AS startup_ts_end,
  st.startup_id,
  st.utid,
  st.tid,
  st.thread_name,
  st.is_main_thread,
  slice.arg_set_id,
  slice.id as slice_id,
  slice.name AS slice_name,
  slice.ts AS slice_ts,
  slice.dur AS slice_dur
FROM android_startup_threads st
JOIN thread_track USING (utid)
JOIN slice ON (slice.track_id = thread_track.id)
WHERE slice.ts BETWEEN st.ts AND st.ts + st.dur;

-- Given a startup id and GLOB for a slice name, returns matching slices with data.
CREATE PERFETTO FUNCTION android_slices_for_startup_and_slice_name(
  -- Startup id.
  startup_id INT,
  -- Glob of the slice.
  slice_name STRING)
RETURNS TABLE(
  -- Id of the slice.
  slice_id INT,
  -- Name of the slice.
  slice_name STRING,
  -- Timestamp of start of the slice.
  slice_ts INT,
  -- Duration of the slice.
  slice_dur INT,
  -- Name of the thread with the slice.
  thread_name STRING,
  -- Tid of the thread with the slice.
  tid  INT,
  -- Arg set id.
  arg_set_id INT
) AS
SELECT slice_id, slice_name, slice_ts, slice_dur, thread_name, tid, arg_set_id
FROM android_thread_slices_for_all_startups
WHERE startup_id = $startup_id AND slice_name GLOB $slice_name;

-- Returns binder transaction slices for a given startup id with duration over threshold.
CREATE PERFETTO FUNCTION android_binder_transaction_slices_for_startup(
  -- Startup id.
  startup_id INT,
  -- Only return slices with duration over threshold.
  threshold DOUBLE)
RETURNS TABLE(
  -- Slice id.
  id INT,
  -- Slice duration.
  slice_dur INT,
  -- Name of the thread with slice.
  thread_name STRING,
  -- Name of the process with slice.
  process STRING,
  -- Arg set id.
  arg_set_id INT,
  -- Whether is main thread.
  is_main_thread BOOL
) AS
SELECT
  slice_id as id,
  slice_dur,
  thread_name,
  process.name as process,
  s.arg_set_id,
  is_main_thread
FROM android_thread_slices_for_all_startups s
JOIN process ON (
  EXTRACT_ARG(s.arg_set_id, "destination process") = process.pid
)
WHERE startup_id = $startup_id
  AND slice_name GLOB "binder transaction"
  AND slice_dur > $threshold;

-- Returns duration of startup for slice name.
--
-- Sums duration of all slices of startup with provided name.
CREATE PERFETTO FUNCTION android_sum_dur_for_startup_and_slice(
  -- Startup id.
  startup_id LONG,
  -- Slice name.
  slice_name STRING)
-- Sum of duration.
RETURNS INT AS
SELECT SUM(slice_dur)
FROM android_thread_slices_for_all_startups
WHERE startup_id = $startup_id
  AND slice_name GLOB $slice_name;

-- Returns duration of startup for slice name on main thread.
--
-- Sums duration of all slices of startup with provided name only on main thread.
CREATE PERFETTO FUNCTION android_sum_dur_on_main_thread_for_startup_and_slice(
  -- Startup id.
  startup_id LONG,
  -- Slice name.
  slice_name STRING)
-- Sum of duration.
RETURNS INT AS
SELECT SUM(slice_dur)
FROM android_thread_slices_for_all_startups
WHERE startup_id = $startup_id
  AND slice_name GLOB $slice_name
  AND is_main_thread;
