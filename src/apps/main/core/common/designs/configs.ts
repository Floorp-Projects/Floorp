/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createSignal, onCleanup, Accessor, Setter } from "solid-js";
import {
  getOldInterfaceConfig,
  getOldTabbarPositionConfig,
  getOldTabbarStyleConfig,
} from "./utils/old-config-migrator";
import {
  type TFloorpDesignConfigs,
  zFloorpDesignConfigs,
} from "../../../../../apps/common/scripts/global-types/type";
import { } from "@core/utils/base";
import { createRootHMR } from "@nora/solid-xul";

const oldObjectConfigs: TFloorpDesignConfigs = {
  globalConfigs: {
    userInterface: getOldInterfaceConfig(),
    faviconColor: Services.prefs.getBoolPref(
      "floorp.titlebar.favicon.color",
      true,
    ),
    appliedUserJs: "",
  },
  tabbar: {
    paddingEnabled: Services.prefs.getBoolPref(
      "floorp.verticaltab.paddingtop.enabled",
      false,
    ),
    tabbarStyle: getOldTabbarStyleConfig(),
    tabbarPosition: getOldTabbarPositionConfig(),
    multiRowTabBar: {
      maxRowEnabled: Services.prefs.getBoolPref(
        "floorp.browser.tabbar.multirow.max.enabled",
        false,
      ),
      maxRow: Services.prefs.getIntPref(
        "floorp.browser.tabbar.multirow.max.row",
        3,
      ),
    },
  },
  tab: {
    tabScroll: {
      enabled: Services.prefs.getBoolPref("floorp.tabscroll.enable", false),
      reverse: Services.prefs.getBoolPref("floorp.tabscroll.reverse", false),
      wrap: Services.prefs.getBoolPref("floorp.tabscroll.wrap", false),
    },
    tabMinHeight: Services.prefs.getIntPref(
      "floorp.browser.tabs.tabMinHeight",
      30,
    ),
    tabMinWidth: Services.prefs.getIntPref("browser.tabs.tabMinWidth", 76),
    tabPinTitle: Services.prefs.getBoolPref(
      "floorp.tabs.showPinnedTabsTitle",
      false,
    ),
    tabDubleClickToClose: Services.prefs.getBoolPref(
      "browser.tabs.closeTabByDblclick",
      false,
    ),
    tabOpenPosition: Services.prefs.getIntPref(
      "floorp.browser.tabs.openTabPosition",
      -1,
    ),
  },
};

export const getOldConfigs = JSON.stringify(oldObjectConfigs);

function createConfig() : [Accessor<TFloorpDesignConfigs>,Setter<TFloorpDesignConfigs>]{
  const [config,setConfig] = createSignal(
    zFloorpDesignConfigs.parse(
      JSON.parse(
        Services.prefs.getStringPref("floorp.design.configs", getOldConfigs),
      ),
    ),
  );
  function updateConfigFromPref() {
    setConfig(
      zFloorpDesignConfigs.parse(
        JSON.parse(Services.prefs.getStringPref("floorp.design.configs")),
      ),
    )
  }

  createEffect(() => {
    Services.prefs.setStringPref(
      "floorp.design.configs",
      JSON.stringify(config()),
    );
  });

  Services.prefs.addObserver("floorp.design.configs", updateConfigFromPref);

  onCleanup(()=>{
    Services.prefs.removeObserver("floorp.design.configs",updateConfigFromPref);
  });
  return [config,setConfig]
}

export const [config,setConfig] = createRootHMR(createConfig,import.meta.hot)

if (!window.gFloorp) {
  window.gFloorp = {};
}
window.gFloorp.designs = {
  setInterface: setBrowserInterface,
};

function setGlobalDesignConfig<
  C extends TFloorpDesignConfigs["globalConfigs"],
  K extends keyof C,
>(key: K, value: C[K]) {
  setConfig((prev) => ({
    ...prev,
    globalConfigs: {
      ...prev.globalConfigs,
      [key]: value,
    },
  }));
}

function setBrowserInterface(
  value: TFloorpDesignConfigs["globalConfigs"]["userInterface"],
) {
  setGlobalDesignConfig("userInterface", value);
}