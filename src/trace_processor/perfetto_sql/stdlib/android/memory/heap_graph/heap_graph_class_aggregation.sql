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

INCLUDE PERFETTO MODULE android.memory.heap_graph.dominator_tree;
INCLUDE PERFETTO MODULE graphs.partition;

CREATE PERFETTO FUNCTION _partition_tree_super_root_fn()
-- The assigned id of the "super root".
RETURNS INT AS
SELECT id + 1
FROM heap_graph_object
ORDER BY id DESC
LIMIT 1;

CREATE PERFETTO FUNCTION _is_libcore_or_array(obj_name STRING)
RETURNS BOOL AS
SELECT ($obj_name GLOB 'java.*' AND NOT $obj_name GLOB 'java.lang.Class<*>')
  OR $obj_name GLOB 'j$.*'
  OR $obj_name GLOB 'int[[]*'
  OR $obj_name GLOB 'long[[]*'
  OR $obj_name GLOB 'byte[[]*'
  OR $obj_name GLOB 'char[[]*'
  OR $obj_name GLOB 'short[[]*'
  OR $obj_name GLOB 'float[[]*'
  OR $obj_name GLOB 'double[[]*'
  OR $obj_name GLOB 'boolean[[]*'
  OR $obj_name GLOB 'android.util.*Array*';

CREATE PERFETTO TABLE _heap_graph_dominator_tree_for_partition AS
SELECT
  tree.id,
  IFNULL(tree.idom_id, _partition_tree_super_root_fn()) as parent_id,
  obj.type_id as group_key
FROM heap_graph_dominator_tree tree
JOIN heap_graph_object obj USING(id)
UNION ALL
-- provide a single root required by tree partition if heap graph exists.
SELECT
  _partition_tree_super_root_fn() AS id,
  NULL AS parent_id,
  (SELECT id + 1 FROM heap_graph_class ORDER BY id desc LIMIT 1) AS group_key
WHERE _partition_tree_super_root_fn() IS NOT NULL;

CREATE PERFETTO TABLE _heap_object_marked_for_dominated_stats AS
SELECT
  id,
  IIF(parent_id IS NULL, 1, 0) as marked
FROM tree_structural_partition_by_group!(_heap_graph_dominator_tree_for_partition)
ORDER BY id;

-- Class-level breakdown of the java heap.
-- Per type name aggregates the object stats and the dominator tree stats.
CREATE PERFETTO TABLE android_heap_graph_class_aggregation (
  -- Process upid
  upid INT,
  -- Heap dump timestamp
  graph_sample_ts INT,
  -- Class type id
  type_id INT,
  -- Class name (deobfuscated if available)
  type_name STRING,
  -- Is type an instance of a libcore object (java.*) or array
  is_libcore_or_array BOOL,
  -- Count of class instances
  obj_count INT,
  -- Size of class instances
  size_bytes INT,
  -- Native size of class instances
  native_size_bytes INT,
  -- Count of reachable class instances
  reachable_obj_count INT,
  -- Size of reachable class instances
  reachable_size_bytes INT,
  -- Native size of reachable class instances
  reachable_native_size_bytes INT,
  -- Count of all objects dominated by instances of this class
  -- Only applies to reachable objects
  dominated_obj_count INT,
  -- Size of all objects dominated by instances of this class
  -- Only applies to reachable objects
  dominated_size_bytes INT,
  -- Native size of all objects dominated by instances of this class
  -- Only applies to reachable objects
  dominated_native_size_bytes INT
) AS
WITH base AS (
  -- First level aggregation to avoid joining with class for every object
  SELECT
    obj.upid,
    obj.graph_sample_ts,
    obj.type_id,
    COUNT(1) AS obj_count,
    SUM(self_size) AS size_bytes,
    SUM(native_size) AS native_size_bytes,
    SUM(IIF(obj.reachable, 1, 0)) AS reachable_obj_count,
    SUM(IIF(obj.reachable, self_size, 0)) AS reachable_size_bytes,
    SUM(IIF(obj.reachable, native_size, 0)) AS reachable_native_size_bytes,
    SUM(IIF(marked, dominated_obj_count, 0)) AS dominated_obj_count,
    SUM(IIF(marked, dominated_size_bytes, 0)) AS dominated_size_bytes,
    SUM(IIF(marked, dominated_native_size_bytes, 0)) AS dominated_native_size_bytes
  FROM heap_graph_object obj
  -- Left joins to preserve unreachable objects.
  LEFT JOIN _heap_object_marked_for_dominated_stats USING(id)
  LEFT JOIN heap_graph_dominator_tree USING(id)
  GROUP BY 1, 2, 3
  ORDER BY 1, 2, 3
)
SELECT
  upid,
  graph_sample_ts,
  type_id,
  IFNULL(cls.deobfuscated_name, cls.name) AS type_name,
  _is_libcore_or_array(IFNULL(cls.deobfuscated_name, cls.name))
    AS is_libcore_or_array,
  SUM(obj_count) AS obj_count,
  SUM(size_bytes) AS size_bytes,
  SUM(native_size_bytes) AS native_size_bytes,
  SUM(reachable_obj_count) AS reachable_obj_count,
  SUM(reachable_size_bytes) AS reachable_size_bytes,
  SUM(reachable_native_size_bytes) AS reachable_native_size_bytes,
  SUM(dominated_obj_count) AS dominated_obj_count,
  SUM(dominated_size_bytes) AS dominated_size_bytes,
  SUM(dominated_native_size_bytes) AS dominated_native_size_bytes
FROM base
JOIN heap_graph_class cls ON base.type_id = cls.id
GROUP BY 1, 2, 3, 4, 5
ORDER BY 1, 2, 3, 4, 5;
