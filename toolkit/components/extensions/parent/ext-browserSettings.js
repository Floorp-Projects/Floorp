/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "AboutNewTab",
  "resource:///modules/AboutNewTab.jsm"
);

var { ExtensionPreferencesManager } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPreferencesManager.jsm"
);

var { ExtensionError } = ExtensionUtils;
var { getSettingsAPI, getPrimedSettingsListener } = ExtensionPreferencesManager;

const HOMEPAGE_URL_PREF = "browser.startup.homepage";

const PERM_DENY_ACTION = Services.perms.DENY_ACTION;

// Add settings objects for supported APIs to the preferences manager.
ExtensionPreferencesManager.addSetting("allowPopupsForUserEvents", {
  permission: "browserSettings",
  prefNames: ["dom.popup_allowed_events"],

  setCallback(value) {
    let returnObj = {};
    // If the value is true, then reset the pref, otherwise set it to "".
    returnObj[this.prefNames[0]] = value ? undefined : "";
    return returnObj;
  },

  getCallback() {
    return Services.prefs.getCharPref("dom.popup_allowed_events") != "";
  },
});

ExtensionPreferencesManager.addSetting("cacheEnabled", {
  permission: "browserSettings",
  prefNames: ["browser.cache.disk.enable", "browser.cache.memory.enable"],

  setCallback(value) {
    let returnObj = {};
    for (let pref of this.prefNames) {
      returnObj[pref] = value;
    }
    return returnObj;
  },

  getCallback() {
    return (
      Services.prefs.getBoolPref("browser.cache.disk.enable") &&
      Services.prefs.getBoolPref("browser.cache.memory.enable")
    );
  },
});

ExtensionPreferencesManager.addSetting("closeTabsByDoubleClick", {
  permission: "browserSettings",
  prefNames: ["browser.tabs.closeTabByDblclick"],

  setCallback(value) {
    return { [this.prefNames[0]]: value };
  },

  getCallback() {
    return Services.prefs.getBoolPref("browser.tabs.closeTabByDblclick");
  },

  validate() {
    if (AppConstants.platform == "android") {
      throw new ExtensionError(
        `android is not a supported platform for the closeTabsByDoubleClick setting.`
      );
    }
  },
});

ExtensionPreferencesManager.addSetting("colorManagement.mode", {
  permission: "browserSettings",
  prefNames: ["gfx.color_management.mode"],

  setCallback(value) {
    switch (value) {
      case "off":
        return { [this.prefNames[0]]: 0 };
      case "full":
        return { [this.prefNames[0]]: 1 };
      case "tagged_only":
        return { [this.prefNames[0]]: 2 };
    }
  },

  getCallback() {
    switch (Services.prefs.getIntPref("gfx.color_management.mode")) {
      case 0:
        return "off";
      case 1:
        return "full";
      case 2:
        return "tagged_only";
    }
  },
});

ExtensionPreferencesManager.addSetting("colorManagement.useNativeSRGB", {
  permission: "browserSettings",
  prefNames: ["gfx.color_management.native_srgb"],

  setCallback(value) {
    return { [this.prefNames[0]]: value };
  },

  getCallback() {
    return Services.prefs.getBoolPref("gfx.color_management.native_srgb");
  },
});

ExtensionPreferencesManager.addSetting(
  "colorManagement.useWebRenderCompositor",
  {
    permission: "browserSettings",
    prefNames: ["gfx.webrender.compositor"],

    setCallback(value) {
      return { [this.prefNames[0]]: value };
    },

    getCallback() {
      return Services.prefs.getBoolPref("gfx.webrender.compositor");
    },
  }
);

ExtensionPreferencesManager.addSetting("contextMenuShowEvent", {
  permission: "browserSettings",
  prefNames: ["ui.context_menus.after_mouseup"],

  setCallback(value) {
    return { [this.prefNames[0]]: value === "mouseup" };
  },

  getCallback() {
    if (AppConstants.platform === "win") {
      return "mouseup";
    }
    let prefValue = Services.prefs.getBoolPref(
      "ui.context_menus.after_mouseup",
      null
    );
    return prefValue ? "mouseup" : "mousedown";
  },
});

ExtensionPreferencesManager.addSetting("imageAnimationBehavior", {
  permission: "browserSettings",
  prefNames: ["image.animation_mode"],

  setCallback(value) {
    return { [this.prefNames[0]]: value };
  },

  getCallback() {
    return Services.prefs.getCharPref("image.animation_mode");
  },
});

ExtensionPreferencesManager.addSetting("newTabPosition", {
  permission: "browserSettings",
  prefNames: [
    "browser.tabs.insertRelatedAfterCurrent",
    "browser.tabs.insertAfterCurrent",
  ],

  setCallback(value) {
    return {
      "browser.tabs.insertAfterCurrent": value === "afterCurrent",
      "browser.tabs.insertRelatedAfterCurrent": value === "relatedAfterCurrent",
    };
  },

  getCallback() {
    if (Services.prefs.getBoolPref("browser.tabs.insertAfterCurrent")) {
      return "afterCurrent";
    }
    if (Services.prefs.getBoolPref("browser.tabs.insertRelatedAfterCurrent")) {
      return "relatedAfterCurrent";
    }
    return "atEnd";
  },
});

ExtensionPreferencesManager.addSetting("openBookmarksInNewTabs", {
  permission: "browserSettings",
  prefNames: ["browser.tabs.loadBookmarksInTabs"],

  setCallback(value) {
    return { [this.prefNames[0]]: value };
  },

  getCallback() {
    return Services.prefs.getBoolPref("browser.tabs.loadBookmarksInTabs");
  },
});

ExtensionPreferencesManager.addSetting("openSearchResultsInNewTabs", {
  permission: "browserSettings",
  prefNames: ["browser.search.openintab"],

  setCallback(value) {
    return { [this.prefNames[0]]: value };
  },

  getCallback() {
    return Services.prefs.getBoolPref("browser.search.openintab");
  },
});

ExtensionPreferencesManager.addSetting("openUrlbarResultsInNewTabs", {
  permission: "browserSettings",
  prefNames: ["browser.urlbar.openintab"],

  setCallback(value) {
    return { [this.prefNames[0]]: value };
  },

  getCallback() {
    return Services.prefs.getBoolPref("browser.urlbar.openintab");
  },
});

ExtensionPreferencesManager.addSetting("webNotificationsDisabled", {
  permission: "browserSettings",
  prefNames: ["permissions.default.desktop-notification"],

  setCallback(value) {
    return { [this.prefNames[0]]: value ? PERM_DENY_ACTION : undefined };
  },

  getCallback() {
    let prefValue = Services.prefs.getIntPref(
      "permissions.default.desktop-notification",
      null
    );
    return prefValue === PERM_DENY_ACTION;
  },
});

ExtensionPreferencesManager.addSetting("overrideDocumentColors", {
  permission: "browserSettings",
  prefNames: ["browser.display.document_color_use"],

  setCallback(value) {
    return { [this.prefNames[0]]: value };
  },

  getCallback() {
    let prefValue = Services.prefs.getIntPref(
      "browser.display.document_color_use"
    );
    if (prefValue === 1) {
      return "never";
    } else if (prefValue === 2) {
      return "always";
    }
    return "high-contrast-only";
  },
});

ExtensionPreferencesManager.addSetting("overrideContentColorScheme", {
  permission: "browserSettings",
  prefNames: ["layout.css.prefers-color-scheme.content-override"],

  setCallback(value) {
    return { [this.prefNames[0]]: value };
  },

  getCallback() {
    let prefValue = Services.prefs.getIntPref(
      "layout.css.prefers-color-scheme.content-override"
    );
    switch (prefValue) {
      case 0:
        return "dark";
      case 1:
        return "light";
      default:
        return "auto";
    }
  },
});

ExtensionPreferencesManager.addSetting("useDocumentFonts", {
  permission: "browserSettings",
  prefNames: ["browser.display.use_document_fonts"],

  setCallback(value) {
    return { [this.prefNames[0]]: value };
  },

  getCallback() {
    return (
      Services.prefs.getIntPref("browser.display.use_document_fonts") !== 0
    );
  },
});

ExtensionPreferencesManager.addSetting("zoomFullPage", {
  permission: "browserSettings",
  prefNames: ["browser.zoom.full"],

  setCallback(value) {
    return { [this.prefNames[0]]: value };
  },

  getCallback() {
    return Services.prefs.getBoolPref("browser.zoom.full");
  },
});

ExtensionPreferencesManager.addSetting("zoomSiteSpecific", {
  permission: "browserSettings",
  prefNames: ["browser.zoom.siteSpecific"],

  setCallback(value) {
    return { [this.prefNames[0]]: value };
  },

  getCallback() {
    return Services.prefs.getBoolPref("browser.zoom.siteSpecific");
  },
});

this.browserSettings = class extends ExtensionAPI {
  homePageOverrideListener(fire) {
    let listener = () => {
      fire.async({
        levelOfControl: "not_controllable",
        value: Services.prefs.getStringPref(HOMEPAGE_URL_PREF),
      });
    };
    Services.prefs.addObserver(HOMEPAGE_URL_PREF, listener);
    return {
      unregister: () => {
        Services.prefs.removeObserver(HOMEPAGE_URL_PREF, listener);
      },
      convert(_fire) {
        fire = _fire;
      },
    };
  }

  newTabOverrideListener(fire) {
    let listener = () => {
      fire.async({
        levelOfControl: "not_controllable",
        value: AboutNewTab.newTabURL,
      });
    };
    Services.obs.addObserver(listener, "newtab-url-changed");
    return {
      unregister: () => {
        Services.obs.removeObserver(listener, "newtab-url-changed");
      },
      convert(_fire) {
        fire = _fire;
      },
    };
  }

  primeListener(event, fire) {
    let { extension } = this;
    if (event == "homepageOverride") {
      return this.homePageOverrideListener(fire);
    }
    if (event == "newTabPageOverride") {
      return this.newTabOverrideListener(fire);
    }
    let listener = getPrimedSettingsListener({
      extension,
      name: event,
    });
    return listener(fire);
  }

  getAPI(context) {
    let self = this;
    let { extension } = context;

    function makeSettingsAPI(name) {
      return getSettingsAPI({
        context,
        module: "browserSettings",
        name,
      });
    }

    return {
      browserSettings: {
        allowPopupsForUserEvents: makeSettingsAPI("allowPopupsForUserEvents"),
        cacheEnabled: makeSettingsAPI("cacheEnabled"),
        closeTabsByDoubleClick: makeSettingsAPI("closeTabsByDoubleClick"),
        contextMenuShowEvent: Object.assign(
          makeSettingsAPI("contextMenuShowEvent"),
          {
            set: details => {
              if (!["mouseup", "mousedown"].includes(details.value)) {
                throw new ExtensionError(
                  `${details.value} is not a valid value for contextMenuShowEvent.`
                );
              }
              if (
                AppConstants.platform === "android" ||
                (AppConstants.platform === "win" &&
                  details.value === "mousedown")
              ) {
                return false;
              }
              return ExtensionPreferencesManager.setSetting(
                extension.id,
                "contextMenuShowEvent",
                details.value
              );
            },
          }
        ),
        ftpProtocolEnabled: getSettingsAPI({
          context,
          name: "ftpProtocolEnabled",
          readOnly: true,
          callback() {
            return false;
          },
        }),
        homepageOverride: getSettingsAPI({
          context,
          // Name differs here to preserve this setting properly
          name: "homepage_override",
          callback() {
            return Services.prefs.getStringPref(HOMEPAGE_URL_PREF);
          },
          readOnly: true,
          onChange: new ExtensionCommon.EventManager({
            context,
            module: "browserSettings",
            event: "homepageOverride",
            name: "homepageOverride.onChange",
            register: fire => {
              return self.homePageOverrideListener(fire).unregister;
            },
          }).api(),
        }),
        imageAnimationBehavior: makeSettingsAPI("imageAnimationBehavior"),
        newTabPosition: makeSettingsAPI("newTabPosition"),
        newTabPageOverride: getSettingsAPI({
          context,
          // Name differs here to preserve this setting properly
          name: "newTabURL",
          callback() {
            return AboutNewTab.newTabURL;
          },
          storeType: "url_overrides",
          readOnly: true,
          onChange: new ExtensionCommon.EventManager({
            context,
            module: "browserSettings",
            event: "newTabPageOverride",
            name: "newTabPageOverride.onChange",
            register: fire => {
              return self.newTabOverrideListener(fire).unregister;
            },
          }).api(),
        }),
        openBookmarksInNewTabs: makeSettingsAPI("openBookmarksInNewTabs"),
        openSearchResultsInNewTabs: makeSettingsAPI(
          "openSearchResultsInNewTabs"
        ),
        openUrlbarResultsInNewTabs: makeSettingsAPI(
          "openUrlbarResultsInNewTabs"
        ),
        webNotificationsDisabled: makeSettingsAPI("webNotificationsDisabled"),
        overrideDocumentColors: Object.assign(
          makeSettingsAPI("overrideDocumentColors"),
          {
            set: details => {
              if (
                !["never", "always", "high-contrast-only"].includes(
                  details.value
                )
              ) {
                throw new ExtensionError(
                  `${details.value} is not a valid value for overrideDocumentColors.`
                );
              }
              let prefValue = 0; // initialize to 0 - auto/high-contrast-only
              if (details.value === "never") {
                prefValue = 1;
              } else if (details.value === "always") {
                prefValue = 2;
              }
              return ExtensionPreferencesManager.setSetting(
                extension.id,
                "overrideDocumentColors",
                prefValue
              );
            },
          }
        ),
        overrideContentColorScheme: Object.assign(
          makeSettingsAPI("overrideContentColorScheme"),
          {
            set: details => {
              let value = details.value;
              if (value == "system" || value == "browser") {
                // Map previous values that used to be different but were
                // unified under the "auto" setting. In practice this should
                // almost always behave like the extension author expects.
                extension.logger.warn(
                  `The "${value}" value for overrideContentColorScheme has been deprecated. Use "auto" instead`
                );
                value = "auto";
              }
              let prefValue = ["dark", "light", "auto"].indexOf(value);
              if (prefValue === -1) {
                throw new ExtensionError(
                  `${value} is not a valid value for overrideContentColorScheme.`
                );
              }
              return ExtensionPreferencesManager.setSetting(
                extension.id,
                "overrideContentColorScheme",
                prefValue
              );
            },
          }
        ),
        useDocumentFonts: Object.assign(makeSettingsAPI("useDocumentFonts"), {
          set: details => {
            if (typeof details.value !== "boolean") {
              throw new ExtensionError(
                `${details.value} is not a valid value for useDocumentFonts.`
              );
            }
            return ExtensionPreferencesManager.setSetting(
              extension.id,
              "useDocumentFonts",
              Number(details.value)
            );
          },
        }),
        zoomFullPage: Object.assign(makeSettingsAPI("zoomFullPage"), {
          set: details => {
            if (typeof details.value !== "boolean") {
              throw new ExtensionError(
                `${details.value} is not a valid value for zoomFullPage.`
              );
            }
            return ExtensionPreferencesManager.setSetting(
              extension.id,
              "zoomFullPage",
              details.value
            );
          },
        }),
        zoomSiteSpecific: Object.assign(makeSettingsAPI("zoomSiteSpecific"), {
          set: details => {
            if (typeof details.value !== "boolean") {
              throw new ExtensionError(
                `${details.value} is not a valid value for zoomSiteSpecific.`
              );
            }
            return ExtensionPreferencesManager.setSetting(
              extension.id,
              "zoomSiteSpecific",
              details.value
            );
          },
        }),
        colorManagement: {
          mode: makeSettingsAPI("colorManagement.mode"),
          useNativeSRGB: makeSettingsAPI("colorManagement.useNativeSRGB"),
          useWebRenderCompositor: makeSettingsAPI(
            "colorManagement.useWebRenderCompositor"
          ),
        },
      },
    };
  }
};
