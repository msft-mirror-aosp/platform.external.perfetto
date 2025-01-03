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

INCLUDE PERFETTO MODULE viz.summary.slices;

CREATE PERFETTO TABLE _track_event_tracks_unordered AS
WITH extracted AS (
  SELECT
    t.id,
    t.name,
    t.parent_id,
    EXTRACT_ARG(t.source_arg_set_id, 'child_ordering') AS ordering,
    EXTRACT_ARG(t.source_arg_set_id, 'sibling_order_rank') AS rank
  FROM track t
  WHERE t.type GLOB '*_track_event'
)
SELECT
  t.id,
  t.name,
  t.parent_id,
  p.ordering AS parent_ordering,
  IFNULL(t.rank, 0) AS rank
FROM extracted t
LEFT JOIN extracted p ON t.parent_id = p.id;

CREATE PERFETTO TABLE _min_ts_per_track AS
SELECT track_id AS id, min(ts) as min_ts
FROM counter
GROUP BY track_id
UNION ALL
SELECT track_id AS id, min(ts) as min_ts
FROM slice
GROUP BY track_id;

CREATE PERFETTO TABLE _track_event_has_children AS
SELECT DISTINCT t.parent_id AS id
FROM track t
WHERE t.type GLOB '*_track_event' AND t.parent_id IS NOT NULL;

CREATE PERFETTO TABLE _track_event_tracks_ordered_groups AS
WITH
  lexicographic_and_none AS (
    SELECT
      id,
      ROW_NUMBER() OVER (PARTITION BY parent_id ORDER BY name) AS order_id
    FROM _track_event_tracks_unordered t
    WHERE t.parent_ordering = 'lexicographic'
      OR t.parent_ordering IS NULL
  ),
  explicit AS (
    SELECT
      id,
      ROW_NUMBER() OVER (PARTITION BY parent_id ORDER BY rank) AS order_id
    FROM _track_event_tracks_unordered t
    WHERE t.parent_ordering = 'explicit'
  ),
  chronological AS (
    SELECT
      t.id,
      ROW_NUMBER() OVER (PARTITION BY t.parent_id ORDER BY m.min_ts) AS order_id
    FROM _track_event_tracks_unordered t
    LEFT JOIN _min_ts_per_track m USING (id)
    WHERE t.parent_ordering = 'chronological'
  ),
  unioned AS (
    SELECT id, order_id
    FROM lexicographic_and_none
    UNION ALL
    SELECT id, order_id
    FROM explicit
    UNION ALL
    SELECT id, order_id
    FROM chronological
  )
SELECT
  extract_arg(track.dimension_arg_set_id, 'upid') AS upid,
  extract_arg(track.dimension_arg_set_id, 'utid') AS utid,
  track.parent_id,
  track.type GLOB '*counter*' AS is_counter,
  track.name,
  MIN(counter_track.unit) AS unit,
  MIN(extract_arg(track.source_arg_set_id, 'builtin_counter_type')) AS builtin_counter_type,
  MAX(m.id IS NOT NULL) AS has_data,
  MAX(c.id IS NOT NULL) AS has_children,
  GROUP_CONCAT(unioned.id) as track_ids,
  MIN(unioned.order_id) AS order_id
FROM unioned
JOIN track USING (id)
LEFT JOIN counter_track USING (id)
LEFT JOIN _track_event_has_children c USING (id)
LEFT JOIN _min_ts_per_track m USING (id)
GROUP BY
  -- Merge by parent id if it exists or, if not, then by upid/utid scope.
  coalesce(track.parent_id, upid, utid),
  is_counter,
  track.name,
  -- Don't merge tracks by name which have children or are counters.
  IIF(c.id IS NOT NULL OR is_counter, track.id, NULL)
ORDER BY track.parent_id, unioned.order_id;