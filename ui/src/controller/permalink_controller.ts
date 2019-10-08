// Copyright (C) 2018 The Android Open Source Project
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

import {Draft, produce} from 'immer';
import * as uuidv4 from 'uuid/v4';

import {assertExists} from '../base/logging';
import {Actions} from '../common/actions';
import {EngineConfig, State} from '../common/state';

import {Controller} from './controller';
import {globals} from './globals';

export const BUCKET_NAME = 'perfetto-ui-data';

function needsToBeUploaded(obj: {}): obj is ArrayBuffer|File {
  return obj instanceof ArrayBuffer || obj instanceof File;
}

export class PermalinkController extends Controller<'main'> {
  private lastRequestId?: string;
  constructor() {
    super('main');
  }

  run() {
    if (globals.state.permalink.requestId === undefined ||
        globals.state.permalink.requestId === this.lastRequestId) {
      return;
    }
    const requestId = assertExists(globals.state.permalink.requestId);
    this.lastRequestId = requestId;

    // if the |link| is not set, this is a request to create a permalink.
    if (globals.state.permalink.hash === undefined) {
      PermalinkController.createPermalink().then(hash => {
        globals.dispatch(Actions.setPermalink({requestId, hash}));
      });
      return;
    }

    // Otherwise, this is a request to load the permalink.
    PermalinkController.loadState(globals.state.permalink.hash).then(state => {
      globals.dispatch(Actions.setState({newState: state}));
      this.lastRequestId = state.permalink.requestId;
    });
  }

  private static async createPermalink() {
    const state = globals.state;

    // Upload each loaded trace.
    const fileToUrl = new Map<File|ArrayBuffer, string>();
    for (const engine of Object.values<EngineConfig>(state.engines)) {
      if (!needsToBeUploaded(engine.source)) continue;
      const name = engine.source instanceof File ? engine.source.name :
                                                   `trace ${engine.id}`;
      PermalinkController.updateStatus(`Uploading ${name}`);
      const url = await this.saveTrace(engine.source);
      fileToUrl.set(engine.source, url);
    }

    // Convert state to use URLs and remove permalink.
    const uploadState = produce(state, draft => {
      for (const engine of Object.values<Draft<EngineConfig>>(
               draft.engines)) {
        if (!needsToBeUploaded(engine.source)) continue;
        const newSource = fileToUrl.get(engine.source);
        if (newSource) engine.source = newSource;
      }
      draft.permalink = {};
    });

    // Upload state.
    PermalinkController.updateStatus(`Creating permalink...`);
    const hash = await this.saveState(uploadState);
    PermalinkController.updateStatus(`Permalink ready`);
    return hash;
  }

  private static async saveState(state: State): Promise<string> {
    const text = JSON.stringify(state);
    const hash = await this.toSha256(text);
    const url = 'https://www.googleapis.com/upload/storage/v1/b/' +
        `${BUCKET_NAME}/o?uploadType=media` +
        `&name=${hash}&predefinedAcl=publicRead`;
    const response = await fetch(url, {
      method: 'post',
      headers: {
        'Content-Type': 'application/json; charset=utf-8',
      },
      body: text,
    });
    await response.json();

    return hash;
  }

  private static async saveTrace(trace: File|ArrayBuffer): Promise<string> {
    // TODO(hjd): This should probably also be a hash but that requires
    // trace processor support.
    const name = uuidv4();
    const url = 'https://www.googleapis.com/upload/storage/v1/b/' +
        `${BUCKET_NAME}/o?uploadType=media` +
        `&name=${name}&predefinedAcl=publicRead`;
    const response = await fetch(url, {
      method: 'post',
      headers: {'Content-Type': 'application/octet-stream;'},
      body: trace,
    });
    await response.json();
    return `https://storage.googleapis.com/${BUCKET_NAME}/${name}`;
  }

  private static async loadState(id: string): Promise<State> {
    const url = `https://storage.googleapis.com/${BUCKET_NAME}/${id}`;
    const response = await fetch(url);
    const text = await response.text();
    const stateHash = await this.toSha256(text);
    const state = JSON.parse(text);
    if (stateHash !== id) {
      throw new Error(`State hash does not match ${id} vs. ${stateHash}`);
    }
    return state;
  }

  private static async toSha256(str: string): Promise<string> {
    // TODO(hjd): TypeScript bug with definition of TextEncoder.
    // tslint:disable-next-line no-any
    const buffer = new (TextEncoder as any)('utf-8').encode(str);
    const digest = await crypto.subtle.digest('SHA-256', buffer);
    return Array.from(new Uint8Array(digest)).map(x => x.toString(16)).join('');
  }

  private static updateStatus(msg: string): void {
    // TODO(hjd): Unify loading updates.
    globals.dispatch(Actions.updateStatus({
      msg,
      timestamp: Date.now() / 1000,
    }));
  }
}
