/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createSignal } from "solid-js";
import {
  defaultEnabled,
  strDefaultConfig,
  strDefaultData,
} from "./utils/default-prerf.js";
import { PanelSidebarStaticNames } from "./utils/panel-sidebar-static-names.js";
import {
  type Panels,
  type PanelSidebarConfig,
  zPanels,
  zPanelSidebarConfig,
} from "./utils/type.js";

/** PanelSidebar data */
export const [panelSidebarData, setPanelSidebarData] = createSignal<Panels>(
  zPanels.parse(
    getPanelSidebarData(
      Services.prefs.getStringPref(
        PanelSidebarStaticNames.panelSidebarDataPrefName,
        strDefaultData,
      ),
    ),
  ),
);

Services.prefs.addObserver(
  PanelSidebarStaticNames.panelSidebarDataPrefName,
  () =>
    setPanelSidebarData(
      zPanels.parse(
        getPanelSidebarData(
          Services.prefs.getStringPref(
            PanelSidebarStaticNames.panelSidebarDataPrefName,
            strDefaultData,
          ),
        ),
      ),
    ),
);

function getPanelSidebarData(stringData: string) {
  return JSON.parse(stringData).data || {};
}

createEffect(() => {
  Services.prefs.setStringPref(
    PanelSidebarStaticNames.panelSidebarDataPrefName,
    JSON.stringify({ data: panelSidebarData() }),
  );
});

/** Selected Panel */
export const [selectedPanelId, setSelectedPanelId] = createSignal<
  string | null
>(null);

createEffect(() => {
  window.gFloorpPanelSidebarCurrentPanel = selectedPanelId();
});

/** Get PanelSidebar Config data */
export const [panelSidebarConfig, setPanelSidebarConfig] =
  createSignal<PanelSidebarConfig>(
    zPanelSidebarConfig.parse(
      getPanelSidebarConfig(
        Services.prefs.getStringPref(
          PanelSidebarStaticNames.panelSidebarConfigPrefName,
          strDefaultConfig,
        ),
      ),
    ),
  );

createEffect(() => {
  Services.prefs.setStringPref(
    PanelSidebarStaticNames.panelSidebarConfigPrefName,
    JSON.stringify({ ...panelSidebarConfig() }),
  );
});

Services.prefs.addObserver(
  PanelSidebarStaticNames.panelSidebarConfigPrefName,
  () =>
    setPanelSidebarConfig(
      zPanelSidebarConfig.parse(
        getPanelSidebarConfig(
          Services.prefs.getStringPref(
            PanelSidebarStaticNames.panelSidebarConfigPrefName,
            strDefaultConfig,
          ),
        ),
      ),
    ),
);

function getPanelSidebarConfig(stringData: string) {
  return JSON.parse(stringData) || {};
}

/** Floating state */
export const [isFloating, setIsFloating] = createSignal(false);

/** Floating DraggingState */
export const [isFloatingDragging, setIsFloatingDragging] =
  createSignal<boolean>(false);

/** Panel Sidebar Enabled */
export const [isPanelSidebarEnabled, setIsPanelSidebarEnabled] =
  createSignal<boolean>(defaultEnabled);

createEffect(() => {
  Services.prefs.setBoolPref(
    PanelSidebarStaticNames.panelSidebarEnabledPrefName,
    isPanelSidebarEnabled(),
  );
});

Services.prefs.addObserver(
  PanelSidebarStaticNames.panelSidebarEnabledPrefName,
  () =>
    setIsPanelSidebarEnabled(
      Services.prefs.getBoolPref(
        PanelSidebarStaticNames.panelSidebarEnabledPrefName,
        defaultEnabled,
      ),
    ),
);
