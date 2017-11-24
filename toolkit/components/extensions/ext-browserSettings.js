/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionSettingsStore",
                                  "resource://gre/modules/ExtensionSettingsStore.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

Cu.import("resource://gre/modules/ExtensionPreferencesManager.jsm");

var {
  ExtensionError,
} = ExtensionUtils;

const HOMEPAGE_OVERRIDE_SETTING = "homepage_override";
const HOMEPAGE_URL_PREF = "browser.startup.homepage";
const URL_STORE_TYPE = "url_overrides";
const NEW_TAB_OVERRIDE_SETTING = "newTabURL";

const PERM_DENY_ACTION = Services.perms.DENY_ACTION;

const getSettingsAPI = (extension, name, callback, storeType, readOnly = false) => {
  return {
    async get(details) {
      return {
        levelOfControl: details.incognito ?
          "not_controllable" :
          await ExtensionPreferencesManager.getLevelOfControl(
            extension, name, storeType),
        value: await callback(),
      };
    },
    set(details) {
      if (!readOnly) {
        return ExtensionPreferencesManager.setSetting(
          extension, name, details.value);
      }
    },
    clear(details) {
      if (!readOnly) {
        return ExtensionPreferencesManager.removeSetting(extension, name);
      }
    },
  };
};

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

ExtensionPreferencesManager.addSetting("imageAnimationBehavior", {
  prefNames: [
    "image.animation_mode",
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

ExtensionPreferencesManager.addSetting("webNotificationsDisabled", {
  prefNames: [
    "permissions.default.desktop-notification",
  ],

  setCallback(value) {
    return {[this.prefNames[0]]: value ? PERM_DENY_ACTION : undefined};
  },
});

this.browserSettings = class extends ExtensionAPI {
  getAPI(context) {
    let {extension} = context;
    return {
      browserSettings: {
        allowPopupsForUserEvents: getSettingsAPI(extension,
          "allowPopupsForUserEvents",
          () => {
            return Services.prefs.getCharPref("dom.popup_allowed_events") != "";
          }),
        cacheEnabled: getSettingsAPI(extension,
          "cacheEnabled",
          () => {
            return Services.prefs.getBoolPref("browser.cache.disk.enable") &&
              Services.prefs.getBoolPref("browser.cache.memory.enable");
          }),
        homepageOverride: getSettingsAPI(extension,
          HOMEPAGE_OVERRIDE_SETTING,
          () => {
            return Services.prefs.getComplexValue(
              HOMEPAGE_URL_PREF, Ci.nsIPrefLocalizedString).data;
          }, undefined, true),
        imageAnimationBehavior: getSettingsAPI(extension,
          "imageAnimationBehavior",
          () => {
            return Services.prefs.getCharPref("image.animation_mode");
          }),
        newTabPageOverride: getSettingsAPI(extension,
          NEW_TAB_OVERRIDE_SETTING,
          () => {
            return aboutNewTabService.newTabURL;
          }, URL_STORE_TYPE, true),
        contextMenuShowEvent: Object.assign(
          getSettingsAPI(
            extension,
            "contextMenuShowEvent",
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
                extension, "contextMenuShowEvent", details.value);
            },
          }
        ),
        webNotificationsDisabled: getSettingsAPI(extension,
          "webNotificationsDisabled",
          () => {
            let prefValue =
              Services.prefs.getIntPref(
                "permissions.default.desktop-notification", null);
            return prefValue === PERM_DENY_ACTION;
          }),
      },
    };
  }
};
