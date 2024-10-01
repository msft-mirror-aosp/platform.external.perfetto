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

import {Time} from '../base/time';
import {AppImpl} from '../core/app_trace_impl';
import {TraceInfo} from '../public/trace_info';
import {EngineBase} from '../trace_processor/engine';

export interface FakeTraceImplArgs {
  // If true suppresses exceptions when trying to issue a query. This is to
  // catch bugs where we are trying to query an empty instance. However some
  // unittests need to do so. Default: false.
  allowQueries?: boolean;
}

// This is used:
// - For testing.
// - By globals.ts before we have an actual trace loaded, to avoid causing
//   if (!= undefined) checks everywhere.
export function createFakeTraceImpl(args: FakeTraceImplArgs = {}) {
  const fakeTraceInfo: TraceInfo = {
    source: {type: 'URL', url: ''},
    traceTitle: '',
    traceUrl: '',
    start: Time.fromSeconds(0),
    end: Time.fromSeconds(10),
    realtimeOffset: Time.ZERO,
    utcOffset: Time.ZERO,
    traceTzOffset: Time.ZERO,
    cpus: [],
    gpuCount: 0,
    importErrors: 0,
  };
  return AppImpl.instance.newTraceInstance(
    new FakeEngine(args.allowQueries ?? false),
    fakeTraceInfo,
  );
}

class FakeEngine extends EngineBase {
  id: string = 'TestEngine';

  constructor(private allowQueries: boolean) {
    super();
  }

  rpcSendRequestBytes(_data: Uint8Array) {
    if (!this.allowQueries) {
      throw new Error(
        'FakeEngine.query() should never be reached. ' +
          'If this is a unittest, try adding {allowQueries: true} to the ' +
          'createFakeTraceImpl() call.',
      );
    }
  }
}
