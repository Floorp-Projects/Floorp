/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  type Accessor,
  createEffect,
  createSignal,
  onCleanup,
  type Setter,
} from "solid-js";
import {
  getOldInterfaceConfig,
  getOldTabbarPositionConfig,
  getOldTabbarStyleConfig,
} from "./utils/old-config-migrator";
import {
  type TFloorpDesignConfigs,
  zFloorpDesignConfigs,
} from "../../../../../apps/common/scripts/global-types/type";
import {} from "@core/utils/base";
import { createRootHMR } from "@nora/solid-xul";

function getOldUICustomizationConfig() {
  const navbarBottom = Services.prefs.getBoolPref(
    "floorp.navbar.bottom",
    false,
  );
  const navPosition = navbarBottom ? "bottom" : "top";

  return {
    navbar: {
      position: navPosition as "bottom" | "top",
      searchBarTop: Services.prefs.getBoolPref("floorp.search.top.mode", false),
    },
    display: {
      disableFullscreenNotification: Services.prefs.getBoolPref(
        "floorp.disable.fullscreen.notification",
        false,
      ),
      deleteBrowserBorder: Services.prefs.getBoolPref(
        "floorp.delete.browser.border",
        false,
      ),
      hideUnifiedExtensionsButton: Services.prefs.getBoolPref(
        "floorp.hide.unifiedExtensionsButtton",
        false,
      ),
    },
    special: {
      optimizeForTreeStyleTab: Services.prefs.getBoolPref(
        "floorp.Tree-type.verticaltab.optimization",
        false,
      ),
      hideForwardBackwardButton: Services.prefs.getBoolPref(
        "floorp.optimized.msbutton.ope",
        false,
      ),
      stgLikeWorkspaces: Services.prefs.getBoolPref(
        "floorp.extensions.STG.like.floorp.workspaces.enabled",
        false,
      ),
    },
    multirowTab: {
      newtabInsideEnabled: Services.prefs.getBoolPref(
        "floorp.browser.tabbar.multirow.newtab-inside.enabled",
        false,
      ),
    },
  };
}

const oldObjectConfigs: TFloorpDesignConfigs = {
  globalConfigs: {
    userInterface: getOldInterfaceConfig(),
    faviconColor: Services.prefs.getBoolPref(
      "floorp.titlebar.favicon.color",
      false,
    ),
    appliedUserJs: "",
  },
  tabbar: {
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
  uiCustomization: getOldUICustomizationConfig(),
};

export const getOldConfigs = JSON.stringify(oldObjectConfigs);

function createConfig(): [
  Accessor<TFloorpDesignConfigs>,
  Setter<TFloorpDesignConfigs>,
] {
  const [config, setConfig] = createSignal(
    zFloorpDesignConfigs.parse(
      JSON.parse(
        Services.prefs.getStringPref("floorp.design.configs", getOldConfigs),
      ),
    ),
  );
  function updateConfigFromPref() {
    try {
      const configStr = Services.prefs.getStringPref("floorp.design.configs");
      const parsedConfig = JSON.parse(configStr);
      setConfig(zFloorpDesignConfigs.parse(parsedConfig));
    } catch (e) {
      console.error("Failed to parse design configs:", e);
      setConfig(zFloorpDesignConfigs.parse(JSON.parse(getOldConfigs)));
    }
  }

  createEffect(() => {
    Services.prefs.setStringPref(
      "floorp.design.configs",
      JSON.stringify(config()),
    );
  });

  Services.prefs.addObserver("floorp.design.configs", updateConfigFromPref);

  onCleanup(() => {
    Services.prefs.removeObserver(
      "floorp.design.configs",
      updateConfigFromPref,
    );
  });
  return [config, setConfig];
}

export const [config, setConfig] = createRootHMR(createConfig, import.meta.hot);

if (!window.gFloorp) {
  window.gFloorp = {};
}
window.gFloorp.designs = {
  setInterface: setBrowserInterface,
};

function setGlobalDesignConfig<
  K extends keyof TFloorpDesignConfigs["globalConfigs"],
>(key: K, value: TFloorpDesignConfigs["globalConfigs"][K]) {
  setConfig((prev) => {
    const newConfig = Object.assign({}, prev);
    newConfig.globalConfigs = Object.assign({}, prev.globalConfigs);
    newConfig.globalConfigs[key] = value;
    return newConfig;
  });
}

function setBrowserInterface(
  value: TFloorpDesignConfigs["globalConfigs"]["userInterface"],
) {
  setGlobalDesignConfig("userInterface", value);
}

export function setUICustomizationConfig<
  K extends keyof TFloorpDesignConfigs["uiCustomization"],
>(category: K, value: TFloorpDesignConfigs["uiCustomization"][K]) {
  setConfig((prev) => {
    const newConfig = Object.assign({}, prev);
    newConfig.uiCustomization = Object.assign({}, prev.uiCustomization);
    newConfig.uiCustomization[category] = value;
    return newConfig;
  });
}

export function updateUICustomizationSetting<
  K extends keyof TFloorpDesignConfigs["uiCustomization"],
  SK extends keyof TFloorpDesignConfigs["uiCustomization"][K],
>(
  category: K,
  setting: SK,
  value: TFloorpDesignConfigs["uiCustomization"][K][SK],
) {
  setConfig((prev) => {
    const newConfig = Object.assign({}, prev);
    newConfig.uiCustomization = Object.assign({}, prev.uiCustomization);

    newConfig.uiCustomization[category] = Object.assign(
      {},
      prev.uiCustomization[category],
    );
    newConfig.uiCustomization[category][setting] = value;

    return newConfig;
  });
}

export function addUICustomizationCategory<T extends Record<string, unknown>>(
  categoryName: string,
  categorySettings: T,
) {
  setConfig((prev) => {
    const newConfig = Object.assign({}, prev);
    newConfig.uiCustomization = Object.assign({}, prev.uiCustomization);
    newConfig.uiCustomization[categoryName] = categorySettings;
    return newConfig;
  });
}

export function getUICustomizationSetting<T>(
  categoryName: string,
  settingName: string,
  defaultValue: T,
): T {
  try {
    const currentConfig = config();
    const uiCustomization = currentConfig.uiCustomization as Record<
      string,
      Record<string, unknown>
    >;

    if (
      categoryName in uiCustomization &&
      settingName in uiCustomization[categoryName]
    ) {
      return uiCustomization[categoryName][settingName] as T;
    }

    return defaultValue;
  } catch (e) {
    console.error(
      `Error getting UI customization setting ${categoryName}.${settingName}:`,
      e,
    );
    return defaultValue;
  }
}
