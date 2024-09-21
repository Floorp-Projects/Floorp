/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createSignal } from "solid-js";
import { SplitViewStaticNames } from "./static-names";
import {
  zSplitViewConfigData,
  type SyncDataGroup,
  type SplitViewDatas,
} from "./type";

/** SplitView data */
export const [splitViewData, setSplitViewData] = createSignal<SplitViewDatas>(
  zSplitViewConfigData.parse(
    getSplitViewData(
      Services.prefs.getStringPref(SplitViewStaticNames.DataPrefName, "{}"),
    ),
  ).splitViewData,
);

/* Current split view */
export const [currentSplitView, setCurrentSplitView] = createSignal<number>(
  getSplitViewData(
    Services.prefs.getStringPref(SplitViewStaticNames.DataPrefName, "{}"),
  ).currentViewIndex,
);

/** Sync data */
export const [syncData, setSyncData] = createSignal(
  zSplitViewConfigData.parse(
    getSplitViewData(
      Services.prefs.getStringPref(SplitViewStaticNames.DataPrefName, "{}"),
    ),
  ).syncData,
);

createEffect(() => {
  Services.prefs.setStringPref(
    SplitViewStaticNames.DataPrefName,
    JSON.stringify({
      splitViewData: splitViewData(),
      syncData: syncData(),
      currentViewIndex: currentSplitView(),
    }),
  );
});

Services.prefs.addObserver(SplitViewStaticNames.DataPrefName, () => {
  setSplitViewData(
    zSplitViewConfigData.parse(
      getSplitViewData(
        Services.prefs.getStringPref(SplitViewStaticNames.DataPrefName, "{}"),
      ),
    ).splitViewData,
  );
  setCurrentSplitView(
    getSplitViewData(
      Services.prefs.getStringPref(SplitViewStaticNames.DataPrefName, "{}"),
    ).currentViewIndex,
  );
  setSyncData(
    zSplitViewConfigData.parse(
      getSplitViewData(
        Services.prefs.getStringPref(SplitViewStaticNames.DataPrefName, "{}"),
      ),
    ).syncData,
  );
});

function getSplitViewData(stringData: string) {
  const splitViewData: SplitViewDatas =
    JSON.parse(stringData).splitViewData || [];
  const syncData: SyncDataGroup = JSON.parse(stringData).syncData || {
    sync: false,
    options: {
      flexType: "flex",
      reverse: false,
      method: "row",
      syncMode: false,
    },
    syncTabId: null,
  };
  const currentViewIndex: number =
    JSON.parse(stringData).currentViewIndex || -1;
  return {
    currentViewIndex,
    syncData,
    splitViewData,
  };
}
