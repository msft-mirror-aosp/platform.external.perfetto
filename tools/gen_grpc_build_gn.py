#!/usr/bin/env python3
# Copyright (C) 2023 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import json
import os
import subprocess
import sys
from typing import Any, Dict, List
import yaml

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

GRPC_GN_HEADER = '''
#
# DO NOT EDIT. AUTOGENERATED file
#
# This file is generated with the command:
# tools/gen_grpc_build_gn.py > buildtools/grpc/BUILD.gn
#

import("../../gn/perfetto.gni")

# Prevent the gRPC from being depended upon without explicitly being opted in.
assert(enable_perfetto_grpc)

# BoringSSL has assembly code which is tied to platform-specific. For now, we
# only care about Linux x64 so assert this as the case.
assert(is_linux && current_cpu == "x64")
'''

TARGET_TEMPLATE = """
{target_type}("{name}") {{
  sources = {srcs}
  public_deps = {deps}
  public_configs = ["..:{config_name}"]
  configs -= [ "//gn/standalone:extra_warnings" ]
}}"""

LIBRARY_IGNORE_LIST = set([
    'grpcpp_channelz',
    'grpc++_reflection',
    'benchmark_helpers',
    'boringssl_test_util',
])

TARGET_ALLOW_LIST = set([
    'grpc_cpp_plugin',
])

STATIC_LIBRARY_TARGETS = set([
    'upb',
    're2',
    'boringssl',
    'grpc++',
])

DEP_DENYLIST = set([
    'cares',
])


def grpc_relpath(*segments: str) -> str:
  '''From path segments to GRPC root, returns the absolute path.'''
  return os.path.join(ROOT_DIR, 'buildtools', 'grpc', 'src', *segments)


GRPC_BUILD_YAML = grpc_relpath('build_autogenerated.yaml')
ABSL_GEN_BUILD_YAML = grpc_relpath('src', 'abseil-cpp', 'gen_build_yaml.py')
BSSL_GEN_BUILD_YAML = grpc_relpath('src', 'boringssl', 'gen_build_yaml.py')


def gen_grpc_dep_yaml(gen_path: str) -> Dict[str, Any]:
  '''Invokes a gen_build_yaml.py file for creating YAML for gRPC deps.'''
  return yaml.safe_load(subprocess.check_output(['python3', gen_path]))


def bazel_label_to_gn_target(dep: str) -> str:
  '''Converts a Bazel label name into a gn target name.'''
  if dep == 'libssl':
    return 'boringssl'
  return dep.replace('/', '_').replace(':', '_')


def bazel_label_to_gn_dep(dep: str) -> str:
  if dep == 'protobuf':
    return '..:protobuf_full'
  if dep == 'protoc':
    return '..:protoc_lib'
  if dep == 'z':
    return '..:zlib'
  return ':' + bazel_label_to_gn_target(dep)


def get_library_target_type(target: str) -> str:
  if target in STATIC_LIBRARY_TARGETS:
    return 'static_library'
  return 'source_set'


def yaml_to_gn_targets(desc: Dict[str, Any], build_types: list[str],
                       config_name: str) -> List[str]:
  '''Given a gRPC YAML description of the build graph, generates GN targets.'''
  out = []
  for lib in desc['libs']:
    if lib['build'] not in build_types:
      continue
    if lib['name'] in LIBRARY_IGNORE_LIST:
      continue
    srcs = [f'src/{file}' for file in lib['src'] + lib['headers']]
    if 'asm_src' in lib:
      srcs += [f'src/{file}' for file in lib['asm_src']['crypto_asm']]
    deps = [
        bazel_label_to_gn_dep(dep)
        for dep in lib.get('deps', [])
        if dep not in DEP_DENYLIST
    ]
    library_target = TARGET_TEMPLATE.format(
        name=bazel_label_to_gn_target(lib['name']),
        config_name=config_name,
        srcs=json.dumps(srcs),
        deps=json.dumps(deps),
        target_type=get_library_target_type(lib['name']))
    out.append(library_target)

  for bin in desc.get('targets', []):
    if bin['build'] not in build_types:
      continue
    if bin['name'] not in TARGET_ALLOW_LIST:
      continue
    srcs = json.dumps([f'src/{file}' for file in bin['src'] + bin['headers']])
    deps = [
        bazel_label_to_gn_dep(dep)
        for dep in lib.get('deps', [])
        if dep not in DEP_DENYLIST
    ]
    binary_target = TARGET_TEMPLATE.format(
        name=bazel_label_to_gn_target(bin['name']),
        config_name=config_name,
        srcs=srcs,
        deps=json.dumps(deps),
        target_type='executable')
    out.append(binary_target)
  return out


def main():
  out: List[str] = []

  # Generate absl rules
  absl_yaml = gen_grpc_dep_yaml(ABSL_GEN_BUILD_YAML)
  out.extend(yaml_to_gn_targets(absl_yaml, ['private'], 'grpc_absl_config'))

  # Generate boringssl rules
  boringssl_yaml = gen_grpc_dep_yaml(BSSL_GEN_BUILD_YAML)
  out.extend(
      yaml_to_gn_targets(boringssl_yaml, ['private'], 'grpc_boringssl_config'))

  # Generate grpc rules
  with open(GRPC_BUILD_YAML, 'r', encoding='utf-8') as f:
    grpc_yaml = yaml.safe_load(f.read())
  out.extend(
      yaml_to_gn_targets(grpc_yaml, ['all', 'protoc'], 'grpc_internal_config'))

  print(GRPC_GN_HEADER)
  print('\n'.join(out))
  return 0


if __name__ == '__main__':
  sys.exit(main())
