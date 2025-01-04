/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Accessor, createEffect, createSignal, onCleanup, Setter } from "solid-js";
import {
  defaultEnabled,
  strDefaultConfig,
  strDefaultData,
} from "../utils/default-prerf.js";
import { PanelSidebarStaticNames } from "../utils/panel-sidebar-static-names.js";
import {
  type Panels,
  type PanelSidebarConfig,
  zPanels,
  zPanelSidebarConfig,
} from "../utils/type.js";
import { createRootHMR } from "@nora/solid-xul";

function createPanelSidebarData(): [Accessor<Panels>,Setter<Panels>] {
  function getPanelSidebarData(stringData: string) {
    return JSON.parse(stringData).data || {};
  }
  const [panelSidebarData, setPanelSidebarData] = createSignal<Panels>(
    zPanels.parse(
      getPanelSidebarData(
        Services.prefs.getStringPref(
          PanelSidebarStaticNames.panelSidebarDataPrefName,
          strDefaultData,
        ),
      ),
    ),
  );
  const observer = () =>
    setPanelSidebarData(
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
    observer
  );
  onCleanup(()=>{
    Services.prefs.removeObserver(PanelSidebarStaticNames.panelSidebarDataPrefName,observer)
  })

  createEffect(() => {
    Services.prefs.setStringPref(
      PanelSidebarStaticNames.panelSidebarDataPrefName,
      JSON.stringify({ data: panelSidebarData() }),
    );
  });
  return [panelSidebarData,setPanelSidebarData]
}

/** PanelSidebar data */
export const [panelSidebarData, setPanelSidebarData] = createRootHMR(createPanelSidebarData,import.meta.hot)

function createSelectedPanelId(): [Accessor<string|null>,Setter<string|null>] {
  const [selectedPanelId, setSelectedPanelId] = createSignal<
    string | null
  >(null);
  createEffect(() => {
    window.gFloorpPanelSidebarCurrentPanel = selectedPanelId();
  });
  return [selectedPanelId, setSelectedPanelId]
}

/** Selected Panel */
export const [selectedPanelId, setSelectedPanelId] = createRootHMR(createSelectedPanelId,import.meta.hot);

function createPanelSidebarConfig(): [Accessor<PanelSidebarConfig>,Setter<PanelSidebarConfig>] {
  const [panelSidebarConfig, setPanelSidebarConfig] =
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
  function getPanelSidebarConfig(stringData: string) {
    return JSON.parse(stringData) || {};
  }
  const observer = () =>
    setPanelSidebarConfig(
      zPanelSidebarConfig.parse(
        getPanelSidebarConfig(
          Services.prefs.getStringPref(
            PanelSidebarStaticNames.panelSidebarConfigPrefName,
            strDefaultConfig,
          ),
        ),
      ),
    );

  Services.prefs.addObserver(
    PanelSidebarStaticNames.panelSidebarConfigPrefName,
    observer
  );
  onCleanup(() => {
    Services.prefs.removeObserver(PanelSidebarStaticNames.panelSidebarConfigPrefName,observer);
  })
  return [panelSidebarConfig, setPanelSidebarConfig]
}

/** Get PanelSidebar Config data */
export const [panelSidebarConfig, setPanelSidebarConfig] = createRootHMR(createPanelSidebarConfig,import.meta.hot)

/** Floating state */
export const [isFloating, setIsFloating] = createRootHMR(()=>createSignal(false),import.meta.hot);

/** Floating DraggingState */
export const [isFloatingDragging, setIsFloatingDragging] =
createRootHMR(()=>createSignal<boolean>(false),import.meta.hot);

function createIsPanelSidebarEnabled(): [Accessor<boolean>,Setter<boolean>] {
  const [isPanelSidebarEnabled, setIsPanelSidebarEnabled] =
  createSignal<boolean>(defaultEnabled);
  createEffect(() => {
    Services.prefs.setBoolPref(
      PanelSidebarStaticNames.panelSidebarEnabledPrefName,
      isPanelSidebarEnabled(),
    );
  });
  const observer = () =>
    setIsPanelSidebarEnabled(
      Services.prefs.getBoolPref(
        PanelSidebarStaticNames.panelSidebarEnabledPrefName,
        defaultEnabled,
      ),
    );
  Services.prefs.addObserver(
    PanelSidebarStaticNames.panelSidebarEnabledPrefName,
    observer
  );
  onCleanup(()=>{
    Services.prefs.removeObserver(PanelSidebarStaticNames.panelSidebarEnabledPrefName,observer)
  }) 

  return [isPanelSidebarEnabled, setIsPanelSidebarEnabled]
}

/** Panel Sidebar Enabled */
export const [isPanelSidebarEnabled, setIsPanelSidebarEnabled] = createRootHMR(createIsPanelSidebarEnabled,import.meta.hot)

