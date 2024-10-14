/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createEffect, createSignal } from "solid-js";
import {
  getOldInterfaceConfig,
  getOldTabbarPositionConfig,
  getOldTabbarStyleConfig,
} from "./old-config-migrator";
import {
  type zFloorpDesignConfigsType,
  zFloorpDesignConfigs,
} from "../../../../../apps/common/scripts/global-types/type";

const oldObjectConfigs: zFloorpDesignConfigsType = {
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
    tabbarScroll: {
      enable: Services.prefs.getBoolPref("floorp.tabscroll.enable", false),
      reverse: Services.prefs.getBoolPref("floorp.tabscroll.reverse", false),
      wrap: Services.prefs.getBoolPref("floorp.tabscroll.wrap", false),
    },
    tabbarMinHeight: Services.prefs.getIntPref(
      "floorp.browser.tabs.tabMinHeight",
      30,
    ),
    tabbarMinWidth: Services.prefs.getIntPref("browser.tabs.tabMinWidth", 76),
    tabbarPinTitle: Services.prefs.getBoolPref(
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

const getOldConfigs = JSON.stringify(oldObjectConfigs);

export const [config, setConfig] = createSignal(
  zFloorpDesignConfigs.parse(
    JSON.parse(
      Services.prefs.getStringPref("floorp.design.configs", getOldConfigs),
    ),
  ),
);

export function setGlobalDesignConfig<
  C extends zFloorpDesignConfigsType["globalConfigs"],
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

export function setBrowserInterface(
  value: zFloorpDesignConfigsType["globalConfigs"]["userInterface"],
) {
  setGlobalDesignConfig("userInterface", value);
}

if (!window.gFloorp) {
  window.gFloorp = {};
}
window.gFloorp.designs = {
  setInterface: setBrowserInterface,
};

createEffect(() => {
  Services.prefs.setStringPref(
    "floorp.design.configs",
    JSON.stringify(config()),
  );
});

Services.prefs.addObserver("floorp.design.configs", () =>
  setConfig(
    zFloorpDesignConfigs.parse(
      JSON.parse(Services.prefs.getStringPref("floorp.design.configs")),
    ),
  ),
);
