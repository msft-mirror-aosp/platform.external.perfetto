// Copyright (C) 2024 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import protos from '../../../protos';
import {assetSrc} from '../../../base/assets';
import {defer} from '../../../base/deferred';
import {assertTrue} from '../../../base/logging';
import {errResult, okResult, Result} from '../../../base/result';
import {utf8Decode, utf8Encode} from '../../../base/string_utils';
import WasmModuleGen from '../../../gen/trace_config_utils';

/**
 * This file is the TS-equivalent of src/trace_config_utils.
 * It exposes two functions to conver the TraceConfig proto from txt<>protobuf.
 * It guarrantees to have the same behaviour of perfetto_cmd and trace_processor
 * by using precisely the same code via WebAssembly.
 */
interface WasmModule {
  module: WasmModuleGen.Module;
  buf: Uint8Array;
}

let moduleInstance: WasmModule | undefined = undefined;

/**
 * Convert a binary-encoded protos.TracConfig to pbtxt (i.e. the text format
 * that can be passed to perfetto --txt).
 */
export async function traceConfigToTxt(
  config: Uint8Array | protos.ITraceConfig,
): Promise<string> {
  const wasm = await initWasmOnce();

  const configU8: Uint8Array =
    config instanceof Uint8Array
      ? config
      : protos.TraceConfig.encode(config).finish();
  assertTrue(configU8.length <= wasm.buf.length);
  wasm.buf.set(configU8);

  const txtSize =
    wasm.module.ccall(
      'trace_config_pb_to_txt',
      'number',
      ['number'],
      [configU8.length],
    ) >>> 0;

  const txt = utf8Decode(wasm.buf.subarray(0, txtSize));
  return txt;
}

/** Convert a pbtxt (text-proto) text to a proto-encoded TraceConfig. */
export async function traceConfigToPb(
  configTxt: string,
): Promise<Result<Uint8Array>> {
  const wasm = await initWasmOnce();

  const configUtf8 = utf8Encode(configTxt);
  assertTrue(configUtf8.length <= wasm.buf.length);
  wasm.buf.set(configUtf8);

  const resSize =
    wasm.module.ccall(
      'trace_config_txt_to_pb',
      'number',
      ['number'],
      [configUtf8.length],
    ) >>> 0;

  const success = wasm.buf.at(0) === 1;
  const payload = wasm.buf.slice(1, 1 + resSize);
  return success ? okResult(payload) : errResult(utf8Decode(payload));
}

async function initWasmOnce(): Promise<WasmModule> {
  if (moduleInstance === undefined) {
    // We have to fetch the .wasm file manually because the stub generated by
    // emscripten uses sync-loading, which works only in Workers.
    const resp = await fetch(assetSrc('trace_config_utils.wasm'));
    const wasmBinary = await resp.arrayBuffer();
    const deferredRuntimeInitialized = defer<void>();
    const instance = WasmModuleGen({
      noInitialRun: true,
      locateFile: (s: string) => s,
      print: (s: string) => console.log(s),
      printErr: (s: string) => console.error(s),
      onRuntimeInitialized: () => deferredRuntimeInitialized.resolve(),
      wasmBinary,
    } as WasmModuleGen.ModuleArgs);
    await deferredRuntimeInitialized;
    const bufAddr =
      instance.ccall('trace_config_utils_buf', 'number', [], []) >>> 0;
    const bufSize =
      instance.ccall('trace_config_utils_buf_size', 'number', [], []) >>> 0;
    moduleInstance = {
      module: instance,
      buf: instance.HEAPU8.subarray(bufAddr, bufAddr + bufSize),
    };
  }
  return moduleInstance;
}
