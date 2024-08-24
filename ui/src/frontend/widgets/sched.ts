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

import m from 'mithril';
import {SchedSqlId} from '../../trace_processor/sql_utils/core_types';
import {duration, time} from '../../base/time';
import {Anchor} from '../../widgets/anchor';
import {Icons} from '../../base/semantic_icons';
import {globals} from '../globals';
import {CPU_SLICE_TRACK_KIND} from '../../core/track_kinds';
import {scrollToTrackAndTs} from '../scroll_helper';
import {exists} from '../../base/utils';

interface SchedRefAttrs {
  id: SchedSqlId;
  ts: time;
  dur: duration;
  cpu: number;
  // If not present, a placeholder name will be used.
  name?: string;

  // Whether clicking on the reference should change the current tab
  // to "current selection" tab in addition to updating the selection
  // and changing the viewport. True by default.
  readonly switchToCurrentSelectionTab?: boolean;
}

export function findSchedTrack(cpu: number): string | undefined {
  for (const track of Object.values(globals.state.tracks)) {
    if (exists(track?.uri)) {
      const trackInfo = globals.trackManager.resolveTrackInfo(track.uri);
      if (trackInfo?.tags?.kind === CPU_SLICE_TRACK_KIND) {
        if (trackInfo?.tags?.cpu === cpu) {
          return track.key;
        }
      }
    }
  }
  return undefined;
}

export function goToSchedSlice(cpu: number, id: SchedSqlId, ts: time) {
  const trackKey = findSchedTrack(cpu);
  if (trackKey === undefined) {
    return;
  }
  globals.setLegacySelection(
    {
      kind: 'SCHED_SLICE',
      id,
      trackKey,
    },
    {
      clearSearch: true,
      pendingScrollId: undefined,
      switchToCurrentSelectionTab: true,
    },
  );

  scrollToTrackAndTs(trackKey, ts);
}

export class SchedRef implements m.ClassComponent<SchedRefAttrs> {
  view(vnode: m.Vnode<SchedRefAttrs>) {
    return m(
      Anchor,
      {
        icon: Icons.UpdateSelection,
        onclick: () => {
          const trackKey = findSchedTrack(vnode.attrs.cpu);
          if (trackKey === undefined) return;

          globals.setLegacySelection(
            {
              kind: 'SCHED_SLICE',
              id: vnode.attrs.id,
              trackKey,
            },
            {
              clearSearch: true,
              pendingScrollId: undefined,
              switchToCurrentSelectionTab:
                vnode.attrs.switchToCurrentSelectionTab ?? true,
            },
          );

          scrollToTrackAndTs(trackKey, vnode.attrs.ts, true);
        },
      },
      vnode.attrs.name ?? `Sched ${vnode.attrs.id}`,
    );
  }
}
