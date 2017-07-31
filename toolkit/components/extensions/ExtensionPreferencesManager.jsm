/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * @fileOverview
 * This module is used for managing preferences from WebExtension APIs.
 * It takes care of the precedence chain and decides whether a preference
 * needs to be updated when a change is requested by an API.
 *
 * It deals with preferences via settings objects, which are objects with
 * the following properties:
 *
 * prefNames:   An array of strings, each of which is a preference on
 *              which the setting depends.
 * setCallback: A function that returns an object containing properties and
 *              values that correspond to the prefs to be set.
 */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionPreferencesManager"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
const {Management} = Cu.import("resource://gre/modules/Extension.jsm", {});

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionSettingsStore",
                                  "resource://gre/modules/ExtensionSettingsStore.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");

XPCOMUtils.defineLazyGetter(this, "defaultPreferences", function() {
  return new Preferences({defaultBranch: true});
});

/* eslint-disable mozilla/balanced-listeners */
Management.on("shutdown", (type, extension) => {
  switch (extension.shutdownReason) {
    case "ADDON_DISABLE":
    case "ADDON_DOWNGRADE":
    case "ADDON_UPGRADE":
      this.ExtensionPreferencesManager.disableAll(extension);
      break;

    case "ADDON_UNINSTALL":
      this.ExtensionPreferencesManager.removeAll(extension);
      break;
  }
});

Management.on("startup", (type, extension) => {
  if (["ADDON_ENABLE", "ADDON_UPGRADE", "ADDON_DOWNGRADE"].includes(extension.startupReason)) {
    this.ExtensionPreferencesManager.enableAll(extension);
  }
});
/* eslint-enable mozilla/balanced-listeners */

const STORE_TYPE = "prefs";

// Definitions of settings, each of which correspond to a different API.
let settingsMap = new Map();

/**
 * This function is passed into the ExtensionSettingsStore to determine the
 * initial value of the setting. It reads an array of preference names from
 * the this scope, which gets bound to a settings object.
 *
 * @returns {Object}
 *          An object with one property per preference, which holds the current
 *          value of that preference.
 */
function initialValueCallback() {
  let initialValue = {};
  for (let pref of this.prefNames) {
    initialValue[pref] = Preferences.get(pref);
  }
  return initialValue;
}

/**
 * Loops through a set of prefs, either setting or resetting them.
 *
 * @param {Object} setting
 *        An object that represents a setting, which will have a setCallback
 *        property.
 * @param {Object} item
 *        An object that represents an item handed back from the setting store
 *        from which the new pref values can be calculated.
*/
function setPrefs(setting, item) {
  let prefs = item.initialValue || setting.setCallback(item.value);
  for (let pref in prefs) {
    if (prefs[pref] === undefined) {
      Preferences.reset(pref);
    } else {
      Preferences.set(pref, prefs[pref]);
    }
  }
}

/**
 * Commits a change to a setting and conditionally sets preferences.
 *
 * If the change to the setting causes a different extension to gain
 * control of the pref (or removes all extensions with control over the pref)
 * then the prefs should be updated, otherwise they should not be.
 * In addition, if the current value of any of the prefs does not
 * match what we expect the value to be (which could be the result of a
 * user manually changing the pref value), then we do not change any
 * of the prefs.
 *
 * @param {Extension} extension
 *        The extension for which a setting is being modified.
 * @param {string} name
 *        The name of the setting being processed.
 * @param {string} action
 *        The action that is being performed. Will be one of disable, enable
 *        or removeSetting.

 * @returns {Promise}
 *          Resolves to true if preferences were set as a result and to false
 *          if preferences were not set.
*/
async function processSetting(extension, name, action) {
  await ExtensionSettingsStore.initialize();
  let expectedItem = ExtensionSettingsStore.getSetting(STORE_TYPE, name);
  let item = ExtensionSettingsStore[action](extension, STORE_TYPE, name);
  if (item) {
    let setting = settingsMap.get(name);
    let expectedPrefs = expectedItem.initialValue
      || setting.setCallback(expectedItem.value);
    if (Object.keys(expectedPrefs).some(
        pref => expectedPrefs[pref] && Preferences.get(pref) != expectedPrefs[pref])) {
      return false;
    }
    setPrefs(setting, item);
    return true;
  }
  return false;
}

this.ExtensionPreferencesManager = {
  /**
   * Adds a setting to the settingsMap. This is how an API tells the
   * preferences manager what its setting object is. The preferences
   * manager needs to know this when settings need to be removed
   * automatically.
   *
   * @param {string} name The unique id of the setting.
   * @param {Object} setting
   *        A setting object that should have properties for
   *        prefNames, getCallback and setCallback.
   */
  addSetting(name, setting) {
    settingsMap.set(name, setting);
  },

  /**
   * Gets the default value for a preference.
   *
   * @param {string} prefName The name of the preference.
   *
   * @returns {string|number|boolean} The default value of the preference.
   */
  getDefaultValue(prefName) {
    return defaultPreferences.get(prefName);
  },

  /**
   * Indicates that an extension would like to change the value of a previously
   * defined setting.
   *
   * @param {Extension} extension
   *        The extension for which a setting is being set.
   * @param {string} name
   *        The unique id of the setting.
   * @param {any} value
   *        The value to be stored in the settings store for this
   *        group of preferences.
   *
   * @returns {Promise}
   *          Resolves to true if the preferences were changed and to false if
   *          the preferences were not changed.
   */
  async setSetting(extension, name, value) {
    let setting = settingsMap.get(name);
    await ExtensionSettingsStore.initialize();
    let item = await ExtensionSettingsStore.addSetting(
      extension, STORE_TYPE, name, value, initialValueCallback.bind(setting));
    if (item) {
      setPrefs(setting, item);
      return true;
    }
    return false;
  },

  /**
   * Indicates that this extension wants to temporarily cede control over the
   * given setting.
   *
   * @param {Extension} extension
   *        The extension for which a preference setting is being removed.
   * @param {string} name
   *        The unique id of the setting.
   *
   * @returns {Promise}
   *          Resolves to true if the preferences were changed and to false if
   *          the preferences were not changed.
   */
  disableSetting(extension, name) {
    return processSetting(extension, name, "disable");
  },

  /**
   * Enable a setting that has been disabled.
   *
   * @param {Extension} extension
   *        The extension for which a setting is being enabled.
   * @param {string} name
   *        The unique id of the setting.
   *
   * @returns {Promise}
   *          Resolves to true if the preferences were changed and to false if
   *          the preferences were not changed.
   */
  enableSetting(extension, name) {
    return processSetting(extension, name, "enable");
  },

  /**
   * Indicates that this extension no longer wants to set the given setting.
   *
   * @param {Extension} extension
   *        The extension for which a preference setting is being removed.
   * @param {string} name
   *        The unique id of the setting.
   *
   * @returns {Promise}
   *          Resolves to true if the preferences were changed and to false if
   *          the preferences were not changed.
   */
  removeSetting(extension, name) {
    return processSetting(extension, name, "removeSetting");
  },

  /**
   * Disables all previously set settings for an extension. This can be called when
   * an extension is being disabled, for example.
   *
   * @param {Extension} extension
   *        The extension for which all settings are being unset.
   */
  async disableAll(extension) {
    await ExtensionSettingsStore.initialize();
    let settings = ExtensionSettingsStore.getAllForExtension(extension, STORE_TYPE);
    let disablePromises = [];
    for (let name of settings) {
      disablePromises.push(this.disableSetting(extension, name));
    }
    await Promise.all(disablePromises);
  },

  /**
   * Enables all disabled settings for an extension. This can be called when
   * an extension has finsihed updating or is being re-enabled, for example.
   *
   * @param {Extension} extension
   *        The extension for which all settings are being enabled.
   */
  async enableAll(extension) {
    await ExtensionSettingsStore.initialize();
    let settings = ExtensionSettingsStore.getAllForExtension(extension, STORE_TYPE);
    let enablePromises = [];
    for (let name of settings) {
      enablePromises.push(this.enableSetting(extension, name));
    }
    await Promise.all(enablePromises);
  },

  /**
   * Removes all previously set settings for an extension. This can be called when
   * an extension is being uninstalled, for example.
   *
   * @param {Extension} extension
   *        The extension for which all settings are being unset.
   */
  async removeAll(extension) {
    await ExtensionSettingsStore.initialize();
    let settings = ExtensionSettingsStore.getAllForExtension(extension, STORE_TYPE);
    let removePromises = [];
    for (let name of settings) {
      removePromises.push(this.removeSetting(extension, name));
    }
    await Promise.all(removePromises);
  },

  /**
   * Return the levelOfControl for a setting / extension combo.
   * This queries the levelOfControl from the ExtensionSettingsStore and also
   * takes into account whether any of the setting's preferences are locked.
   *
   * @param {Extension} extension
   *        The extension for which levelOfControl is being requested.
   * @param {string} name
   *        The unique id of the setting.
   *
   * @returns {Promise}
   *          Resolves to the level of control of the extension over the setting.
   */
  async getLevelOfControl(extension, name) {
    for (let prefName of settingsMap.get(name).prefNames) {
      if (Preferences.locked(prefName)) {
        return "not_controllable";
      }
    }
    await ExtensionSettingsStore.initialize();
    return ExtensionSettingsStore.getLevelOfControl(extension, STORE_TYPE, name);
  },
};
