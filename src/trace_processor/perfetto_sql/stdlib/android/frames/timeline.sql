--
-- Copyright 2024 The Android Open Source Project
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

-- Parses the slice name to fetch `frame_id` from `slice` table.
-- Use with caution. Slice names are a flaky source of ids and the resulting
-- table might require some further operations.
CREATE PERFETTO FUNCTION _get_frame_table_with_id(
    -- String just before id.
    glob_str STRING
) RETURNS TABLE (
    -- `slice.id` of the frame slice.
    id INT,
    -- Parsed frame id.
    frame_id INT
) AS
WITH all_found AS (
    SELECT
        id,
        cast_int!(STR_SPLIT(name, ' ', 1)) AS frame_id
    FROM slice
    WHERE name GLOB $glob_str
)
SELECT *
FROM all_found
-- Casting string to int returns 0 if the string can't be cast.
WHERE frame_id != 0;

-- All of the `Choreographer#doFrame` slices with their frame id.
CREATE PERFETTO TABLE android_frames_choreographer_do_frame(
    -- `slice.id`
    id INT,
    -- Frame id
    frame_id INT,
    -- Utid of the UI thread
    ui_thread_utid INT
) AS
SELECT
    c.*,
    thread_track.utid AS ui_thread_utid
FROM _get_frame_table_with_id('Choreographer#doFrame*') c
JOIN slice USING (id)
JOIN thread_track ON (thread_track.id = slice.track_id);

-- All of the `DrawFrame` slices with their frame id and render thread.
-- There might be multiple DrawFrames slices for a single vsync (frame id).
-- This happens when we are drawing multiple layers (e.g. status bar and
-- notifications).
CREATE PERFETTO TABLE android_frames_draw_frame(
    -- `slice.id`
    id INT,
    -- Frame id
    frame_id INT,
    -- Utid of the render thread
    render_thread_utid INT
) AS
SELECT
    d.*,
    thread_track.utid AS render_thread_utid
FROM _get_frame_table_with_id('DrawFrame*') d
JOIN slice USING (id)
JOIN thread_track ON (thread_track.id = slice.track_id);

-- `actual_frame_timeline_slice` returns the same slice on different tracks.
-- We are getting the first slice with one frame id.
CREATE PERFETTO TABLE _distinct_from_actual_timeline_slice AS
SELECT
    id,
    cast_int!(name) AS frame_id,
    ts,
    dur
FROM actual_frame_timeline_slice
GROUP BY 2;

-- `expected_frame_timeline_slice` returns the same slice on different tracks.
-- We are getting the first slice with one frame id.
CREATE PERFETTO TABLE _distinct_from_expected_timeline_slice AS
SELECT
    id,
    cast_int!(name) AS frame_id
FROM expected_frame_timeline_slice
GROUP BY 2;

-- All slices related to one frame. Aggregates `Choreographer#doFrame`,
-- `DrawFrame`, `actual_frame_timeline_slice` and
-- `expected_frame_timeline_slice` slices.
-- See https://perfetto.dev/docs/data-sources/frametimeline for details.
CREATE PERFETTO TABLE android_frames(
    -- Frame id.
    frame_id INT,
    -- Timestamp of the frame. Start of the frame as defined by the start of
    -- "Choreographer#doFrame" slice and the same as the start of the frame in
    -- `actual_frame_timeline_slice .
    ts INT,
    -- Duration of the frame, as defined by the duration of the corresponding
    -- `actual_frame_timeline_slice` duration.
    dur INT,
    -- `slice.id` of "Choreographer#doFrame" slice.
    do_frame_id INT,
    -- `slice.id` of "DrawFrame" slice.
    draw_frame_id INT,
    -- `slice.id` from `actual_frame_timeline_slice`
    actual_frame_timeline_id INT,
    -- `slice.id` from `expected_frame_timeline_slice`
    expected_frame_timeline_id INT,
    -- `utid` of the render thread.
    render_thread_utid INT,
    -- `utid` of the UI thread.
    ui_thread_utid INT
) AS
SELECT
    frame_id,
    ts,
    dur,
    do_frame.id AS do_frame_id,
    draw_frame.id AS draw_frame_id,
    act.id AS actual_frame_timeline_id,
    exp.id AS expected_frame_timeline_id,
    draw_frame.render_thread_utid,
    do_frame.ui_thread_utid
FROM android_frames_choreographer_do_frame do_frame
JOIN android_frames_draw_frame draw_frame USING (frame_id)
JOIN _distinct_from_actual_timeline_slice act USING (frame_id)
JOIN _distinct_from_expected_timeline_slice exp USING (frame_id)
ORDER BY frame_id;

-- Returns first frame after the provided timestamp. The returning table has at
-- most one row.
CREATE PERFETTO FUNCTION android_first_frame_after(
    -- Timestamp.
    ts INT)
RETURNS TABLE (
    -- Frame id.
    frame_id INT,
    -- Start of the frame, the timestamp of the "Choreographer#doFrame" slice.
    ts INT,
    -- Duration of the frame.
    dur INT,
    -- `slice.id` of "Choreographer#doFrame" slice.
    do_frame_id INT,
    -- `slice.id` of "DrawFrame" slice.
    draw_frame_id INT,
    -- `slice.id` from `actual_frame_timeline_slice`
    actual_frame_timeline_id INT,
    -- `slice.id` from `expected_frame_timeline_slice`
    expected_frame_timeline_id INT,
    -- `utid` of the render thread.
    render_thread_utid INT,
    -- `utid` of the UI thread.
    ui_thread_utid INT
) AS
SELECT * FROM android_frames
WHERE ts > $ts
ORDER BY ts
LIMIT 1;