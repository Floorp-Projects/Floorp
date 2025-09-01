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

function isPlainObject(value: unknown): value is Record<string, unknown> {
  return (
    typeof value === "object" &&
    value !== null &&
    !Array.isArray(value)
  );
}

function deepMerge<T extends Record<string, unknown>>(base: T, override: unknown): T {
  // Start from a deep clone of base to avoid mutations
  const result: T = isPlainObject(base)
    ? (JSON.parse(JSON.stringify(base)) as T)
    : base;

  if (!isPlainObject(override)) {
    return result;
  }

  const res = result as unknown as Record<string, unknown>;

  for (const [key, overrideVal] of Object.entries(override)) {
    const baseVal = res[key];
    if (isPlainObject(baseVal) && isPlainObject(overrideVal)) {
      res[key] = deepMerge(baseVal as Record<string, unknown>, overrideVal);
    } else if (overrideVal !== undefined) {
      res[key] = overrideVal as unknown;
    }
  }

  return result;
}

function getOldUICustomizationConfig() {
  try {
    const navbarBottom = Services.prefs.getBoolPref(
      "floorp.navbar.bottom",
      false,
    );
    const navPosition = navbarBottom ? "bottom" : "top";

    return {
      navbar: {
        position: navPosition as "bottom" | "top",
        searchBarTop: Services.prefs.getBoolPref(
          "floorp.search.top.mode",
          false,
        ),
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
      bookmarkBar: {
        focusExpand: Services.prefs.getBoolPref(
          "floorp.bookmarks.bar.focus.mode",
          false,
        ),
      },
      qrCode: {
        disableButton: false,
      },
    };
  } catch (e) {
    console.error("Failed to get UI customization config:", e);
    return {
      navbar: {
        position: "top" as "bottom" | "top",
        searchBarTop: false,
      },
      display: {
        disableFullscreenNotification: false,
        deleteBrowserBorder: false,
        hideUnifiedExtensionsButton: false,
      },
      special: {
        optimizeForTreeStyleTab: false,
        hideForwardBackwardButton: false,
        stgLikeWorkspaces: false,
      },
      multirowTab: {
        newtabInsideEnabled: false,
      },
      bookmarkBar: {
        focusExpand: false,
      },
      qrCode: {
        disableButton: false,
      },
    };
  }
}

function createDefaultOldObjectConfigs(): TFloorpDesignConfigs {
  try {
    return {
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
          reverse: Services.prefs.getBoolPref(
            "floorp.tabscroll.reverse",
            false,
          ),
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
  } catch (e) {
    console.error("Failed to create default old object configs:", e);
    return {
      globalConfigs: {
        userInterface: "proton",
        faviconColor: false,
        appliedUserJs: "",
      },
      tabbar: {
        tabbarStyle: "horizontal",
        tabbarPosition: "default",
        multiRowTabBar: {
          maxRowEnabled: false,
          maxRow: 3,
        },
      },
      tab: {
        tabScroll: {
          enabled: false,
          reverse: false,
          wrap: false,
        },
        tabMinHeight: 30,
        tabMinWidth: 76,
        tabPinTitle: false,
        tabDubleClickToClose: false,
        tabOpenPosition: -1,
      },
      uiCustomization: {
        navbar: {
          position: "top",
          searchBarTop: false,
        },
        display: {
          disableFullscreenNotification: false,
          deleteBrowserBorder: false,
          hideUnifiedExtensionsButton: false,
        },
        special: {
          optimizeForTreeStyleTab: false,
          hideForwardBackwardButton: false,
          stgLikeWorkspaces: false,
        },
        multirowTab: {
          newtabInsideEnabled: false,
        },
        bookmarkBar: {
          focusExpand: false,
        },
        qrCode: {
          disableButton: false,
        },
      },
    };
  }
}

const oldObjectConfigs = createDefaultOldObjectConfigs();

export const getOldConfigs = JSON.stringify(oldObjectConfigs);

function createConfig(): [
  Accessor<TFloorpDesignConfigs>,
  Setter<TFloorpDesignConfigs>,
] {
  const defaultConfig = zFloorpDesignConfigs.parse(JSON.parse(getOldConfigs));

  let initialConfig = defaultConfig;
  try {
    const configStr = Services.prefs.getStringPref(
      "floorp.design.configs",
      getOldConfigs,
    );
    const parsedConfig = JSON.parse(configStr);
    // Merge existing config with defaults to tolerate newly added fields
    const merged = deepMerge(defaultConfig, parsedConfig);
    initialConfig = zFloorpDesignConfigs.parse(merged);
  } catch (e) {
    console.error("Failed to parse initial design configs, using defaults:", e);
  }

  const [config, setConfig] = createSignal(initialConfig);

  function updateConfigFromPref() {
    try {
      const configStr = Services.prefs.getStringPref(
        "floorp.design.configs",
        getOldConfigs,
      );
      const parsedConfig = JSON.parse(configStr);
      const merged = deepMerge(defaultConfig, parsedConfig);
      setConfig(zFloorpDesignConfigs.parse(merged));
    } catch (e) {
      console.error("Failed to parse design configs:", e);
    }
  }

  createEffect(() => {
    try {
      Services.prefs.setStringPref(
        "floorp.design.configs",
        JSON.stringify(config()),
      );
    } catch (e) {
      console.error("Failed to save design configs:", e);
    }
  });

  try {
    Services.prefs.addObserver("floorp.design.configs", updateConfigFromPref);

    onCleanup(() => {
      try {
        Services.prefs.removeObserver(
          "floorp.design.configs",
          updateConfigFromPref,
        );
      } catch (e) {
        console.error("Failed to remove observer:", e);
      }
    });
  } catch (e) {
    console.error("Failed to add observer:", e);
  }

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
  try {
    setConfig((prev) => {
      const newConfig = Object.assign({}, prev);
      newConfig.globalConfigs = Object.assign({}, prev.globalConfigs);
      newConfig.globalConfigs[key] = value;
      return newConfig;
    });
  } catch (e) {
    console.error(
      `Failed to set global design config for key ${String(key)}:`,
      e,
    );
  }
}

function setBrowserInterface(
  value: TFloorpDesignConfigs["globalConfigs"]["userInterface"],
) {
  setGlobalDesignConfig("userInterface", value);
}

export function setUICustomizationConfig<
  K extends keyof TFloorpDesignConfigs["uiCustomization"],
>(category: K, value: TFloorpDesignConfigs["uiCustomization"][K]) {
  try {
    setConfig((prev) => {
      const newConfig = Object.assign({}, prev);
      newConfig.uiCustomization = Object.assign({}, prev.uiCustomization);
      newConfig.uiCustomization[category] = value;
      return newConfig;
    });
  } catch (e) {
    console.error(
      `Failed to set UI customization config for category ${String(category)}:`,
      e,
    );
  }
}

export function updateUICustomizationSetting<
  K extends keyof TFloorpDesignConfigs["uiCustomization"],
  SK extends keyof TFloorpDesignConfigs["uiCustomization"][K],
>(
  category: K,
  setting: SK,
  value: TFloorpDesignConfigs["uiCustomization"][K][SK],
) {
  try {
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
  } catch (e) {
    console.error(
      `Failed to update UI customization setting ${String(category)}.${
        String(setting)
      }:`,
      e,
    );
  }
}

export function addUICustomizationCategory<T extends Record<string, unknown>>(
  categoryName: string,
  categorySettings: T,
) {
  try {
    setConfig((prev) => {
      const newConfig = Object.assign({}, prev);
      newConfig.uiCustomization = Object.assign({}, prev.uiCustomization);
      newConfig.uiCustomization[categoryName] = categorySettings;
      return newConfig;
    });
  } catch (e) {
    console.error(
      `Failed to add UI customization category ${categoryName}:`,
      e,
    );
  }
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
      uiCustomization &&
      categoryName in uiCustomization &&
      uiCustomization[categoryName] &&
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
