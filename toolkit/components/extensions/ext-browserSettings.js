/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionSettingsStore",
                                  "resource://gre/modules/ExtensionSettingsStore.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");

Cu.import("resource://gre/modules/ExtensionPreferencesManager.jsm");

const HOMEPAGE_OVERRIDE_SETTING = "homepage_override";
const URL_STORE_TYPE = "url_overrides";
const NEW_TAB_OVERRIDE_SETTING = "newTabURL";

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

this.browserSettings = class extends ExtensionAPI {
  getAPI(context) {
    let {extension} = context;
    return {
      browserSettings: {
        allowPopupsForUserEvents: getSettingsAPI(extension,
          "allowPopupsForUserEvents",
          () => {
            return Preferences.get("dom.popup_allowed_events") != "";
          }),
        cacheEnabled: getSettingsAPI(extension,
          "cacheEnabled",
          () => {
            return Preferences.get("browser.cache.disk.enable") &&
              Preferences.get("browser.cache.memory.enable");
          }),
        homepageOverride: getSettingsAPI(extension,
          HOMEPAGE_OVERRIDE_SETTING,
          async () => {
            let homepageSetting = await ExtensionPreferencesManager.getSetting(HOMEPAGE_OVERRIDE_SETTING);
            if (homepageSetting) {
              return homepageSetting.value;
            }
            return null;
          }, undefined, true),
        newTabPageOverride: getSettingsAPI(extension,
          NEW_TAB_OVERRIDE_SETTING,
          async () => {
            await ExtensionSettingsStore.initialize();
            let newTabPageSetting = ExtensionSettingsStore.getSetting(URL_STORE_TYPE, NEW_TAB_OVERRIDE_SETTING);
            if (newTabPageSetting) {
              return newTabPageSetting.value;
            }
            return null;
          }, URL_STORE_TYPE, true),
      },
    };
  }
};
