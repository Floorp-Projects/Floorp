/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");

Cu.import("resource://gre/modules/ExtensionPreferencesManager.jsm");

const getSettingsAPI = (extension, name, callback) => {
  return {
    async get(details) {
      return {
        levelOfControl: details.incognito ?
          "not_controllable" :
          await ExtensionPreferencesManager.getLevelOfControl(
            extension, name),
        value: await callback(),
      };
    },
    set(details) {
      return ExtensionPreferencesManager.setSetting(
        extension, name, details.value);
    },
    clear(details) {
      return ExtensionPreferencesManager.removeSetting(extension, name);
    },
  };
};

// Add settings objects for supported APIs to the preferences manager.
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
        cacheEnabled: getSettingsAPI(extension,
          "cacheEnabled",
          () => {
            return Preferences.get("browser.cache.disk.enable") &&
              Preferences.get("browser.cache.memory.enable");
          }),
      },
    };
  }
};
