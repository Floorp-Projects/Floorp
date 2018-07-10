/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(this, "AppConstants",
                               "resource://gre/modules/AppConstants.jsm");
ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

ChromeUtils.import("resource://gre/modules/ExtensionPreferencesManager.jsm");

var {
  ExtensionError,
} = ExtensionUtils;
var {
  getSettingsAPI,
} = ExtensionPreferencesManager;

const HOMEPAGE_OVERRIDE_SETTING = "homepage_override";
const HOMEPAGE_URL_PREF = "browser.startup.homepage";
const URL_STORE_TYPE = "url_overrides";
const NEW_TAB_OVERRIDE_SETTING = "newTabURL";

const PERM_DENY_ACTION = Services.perms.DENY_ACTION;

// Add settings objects for supported APIs to the preferences manager.
ExtensionPreferencesManager.addSetting("allowPopupsForUserEvents", {
  prefNames: [
    "dom.popup_allowed_events",
  ],

  setCallback(value) {
    let returnObj = {};
    // If the value is true, then reset the pref, otherwise set it to "".
    returnObj[this.prefNames[0]] = value ? undefined : "";
    return returnObj;
  },
});

ExtensionPreferencesManager.addSetting("cacheEnabled", {
  prefNames: [
    "browser.cache.disk.enable",
    "browser.cache.memory.enable",
  ],

  setCallback(value) {
    let returnObj = {};
    for (let pref of this.prefNames) {
      returnObj[pref] = value;
    }
    return returnObj;
  },
});

ExtensionPreferencesManager.addSetting("closeTabsByDoubleClick", {
  prefNames: [
    "browser.tabs.closeTabByDblclick",
  ],

  setCallback(value) {
    return {[this.prefNames[0]]: value};
  },
});

ExtensionPreferencesManager.addSetting("contextMenuShowEvent", {
  prefNames: [
    "ui.context_menus.after_mouseup",
  ],

  setCallback(value) {
    return {[this.prefNames[0]]: value === "mouseup"};
  },
});

ExtensionPreferencesManager.addSetting("imageAnimationBehavior", {
  prefNames: [
    "image.animation_mode",
  ],

  setCallback(value) {
    return {[this.prefNames[0]]: value};
  },
});

ExtensionPreferencesManager.addSetting("newTabPosition", {
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
});

ExtensionPreferencesManager.addSetting("openBookmarksInNewTabs", {
  prefNames: [
    "browser.tabs.loadBookmarksInTabs",
  ],

  setCallback(value) {
    return {[this.prefNames[0]]: value};
  },
});

ExtensionPreferencesManager.addSetting("openSearchResultsInNewTabs", {
  prefNames: [
    "browser.search.openintab",
  ],

  setCallback(value) {
    return {[this.prefNames[0]]: value};
  },
});

ExtensionPreferencesManager.addSetting("openUrlbarResultsInNewTabs", {
  prefNames: [
    "browser.urlbar.openintab",
  ],

  setCallback(value) {
    return {[this.prefNames[0]]: value};
  },
});

ExtensionPreferencesManager.addSetting("webNotificationsDisabled", {
  prefNames: [
    "permissions.default.desktop-notification",
  ],

  setCallback(value) {
    return {[this.prefNames[0]]: value ? PERM_DENY_ACTION : undefined};
  },
});

ExtensionPreferencesManager.addSetting("overrideDocumentColors", {
  prefNames: [
    "browser.display.document_color_use",
  ],

  setCallback(value) {
    return {[this.prefNames[0]]: value};
  },
});

ExtensionPreferencesManager.addSetting("useDocumentFonts", {
  prefNames: [
    "browser.display.use_document_fonts",
  ],

  setCallback(value) {
    return {[this.prefNames[0]]: value};
  },
});

this.browserSettings = class extends ExtensionAPI {
  getAPI(context) {
    let {extension} = context;
    return {
      browserSettings: {
        allowPopupsForUserEvents: getSettingsAPI(
          extension.id, "allowPopupsForUserEvents",
          () => {
            return Services.prefs.getCharPref("dom.popup_allowed_events") != "";
          }),
        cacheEnabled: getSettingsAPI(
          extension.id, "cacheEnabled",
          () => {
            return Services.prefs.getBoolPref("browser.cache.disk.enable") &&
              Services.prefs.getBoolPref("browser.cache.memory.enable");
          }),
        closeTabsByDoubleClick: getSettingsAPI(
          extension.id, "closeTabsByDoubleClick",
          () => {
            return Services.prefs.getBoolPref("browser.tabs.closeTabByDblclick");
          }, undefined, false, () => {
            if (AppConstants.platform == "android") {
              throw new ExtensionError(
                `android is not a supported platform for the closeTabsByDoubleClick setting.`);
            }
          }),
        contextMenuShowEvent: Object.assign(
          getSettingsAPI(
            extension.id, "contextMenuShowEvent",
            () => {
              if (AppConstants.platform === "win") {
                return "mouseup";
              }
              let prefValue = Services.prefs.getBoolPref(
                "ui.context_menus.after_mouseup", null);
              return prefValue ? "mouseup" : "mousedown";
            }
          ),
          {
            set: details => {
              if (!["mouseup", "mousedown"].includes(details.value)) {
                throw new ExtensionError(
                  `${details.value} is not a valid value for contextMenuShowEvent.`);
              }
              if (AppConstants.platform === "android" ||
                  (AppConstants.platform === "win" &&
                   details.value === "mousedown")) {
                return false;
              }
              return ExtensionPreferencesManager.setSetting(
                extension.id, "contextMenuShowEvent", details.value);
            },
          }
        ),
        homepageOverride: getSettingsAPI(
          extension.id, HOMEPAGE_OVERRIDE_SETTING,
          () => {
            return Services.prefs.getStringPref(HOMEPAGE_URL_PREF);
          }, undefined, true),
        imageAnimationBehavior: getSettingsAPI(
          extension.id, "imageAnimationBehavior",
          () => {
            return Services.prefs.getCharPref("image.animation_mode");
          }),
        newTabPosition: getSettingsAPI(
          extension.id, "newTabPosition",
          () => {
            if (Services.prefs.getBoolPref("browser.tabs.insertAfterCurrent")) {
              return "afterCurrent";
            }
            if (Services.prefs.getBoolPref("browser.tabs.insertRelatedAfterCurrent")) {
              return "relatedAfterCurrent";
            }
            return "atEnd";
          }),
        newTabPageOverride: getSettingsAPI(
          extension.id, NEW_TAB_OVERRIDE_SETTING,
          () => {
            return aboutNewTabService.newTabURL;
          }, URL_STORE_TYPE, true),
        openBookmarksInNewTabs: getSettingsAPI(
          extension.id, "openBookmarksInNewTabs",
          () => {
            return Services.prefs.getBoolPref("browser.tabs.loadBookmarksInTabs");
          }),
        openSearchResultsInNewTabs: getSettingsAPI(
          extension.id, "openSearchResultsInNewTabs",
          () => {
            return Services.prefs.getBoolPref("browser.search.openintab");
          }),
        openUrlbarResultsInNewTabs: getSettingsAPI(
          extension.id, "openUrlbarResultsInNewTabs",
          () => {
            return Services.prefs.getBoolPref("browser.urlbar.openintab");
          }),
        webNotificationsDisabled: getSettingsAPI(
          extension.id, "webNotificationsDisabled",
          () => {
            let prefValue =
              Services.prefs.getIntPref(
                "permissions.default.desktop-notification", null);
            return prefValue === PERM_DENY_ACTION;
          }),
        overrideDocumentColors: Object.assign(
          getSettingsAPI(
            extension.id, "overrideDocumentColors",
            () => {
              let prefValue = Services.prefs.getIntPref("browser.display.document_color_use");
              if (prefValue === 1) {
                return "never";
              } else if (prefValue === 2) {
                return "always";
              }
              return "high-contrast-only";
            }
          ),
          {
            set: details => {
              if (!["never", "always", "high-contrast-only"].includes(details.value)) {
                throw new ExtensionError(
                  `${details.value} is not a valid value for overrideDocumentColors.`);
              }
              let prefValue = 0; // initialize to 0 - auto/high-contrast-only
              if (details.value === "never") {
                prefValue = 1;
              } else if (details.value === "always") {
                prefValue = 2;
              }
              return ExtensionPreferencesManager.setSetting(
                extension.id, "overrideDocumentColors", prefValue);
            },
          }
        ),
        useDocumentFonts: Object.assign(
          getSettingsAPI(
            extension.id, "useDocumentFonts",
            () => {
              return Services.prefs.getIntPref("browser.display.use_document_fonts") !== 0;
            }
          ),
          {
            set: details => {
              if (typeof details.value !== "boolean") {
                throw new ExtensionError(
                  `${details.value} is not a valid value for useDocumentFonts.`);
              }
              return ExtensionPreferencesManager.setSetting(
                extension.id, "useDocumentFonts", Number(details.value));
            },
          }
        ),
      },
    };
  }
};
