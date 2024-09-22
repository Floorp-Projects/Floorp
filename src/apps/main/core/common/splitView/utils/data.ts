/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createSignal } from "solid-js";
import type { SyncDataGroup, SplitViewDatas } from "./type";

/** SplitView data */
export const [splitViewData, setSplitViewData] = createSignal<SplitViewDatas>(
  [],
);

/* Current split view */
export const [currentSplitView, setCurrentSplitView] = createSignal<number>(-1);

/** Sync data */
export const [syncData, setSyncData] = createSignal<SyncDataGroup>({
  sync: false,
  options: {
    reverse: false,
    method: "row",
    syncMode: false,
  },
  syncTabId: null,
});

createEffect(() => {
  console.error(
    "splitViewData",
    splitViewData(),
    "syncData",
    syncData(),
    "currentSplitView",
    currentSplitView(),
  );
});
