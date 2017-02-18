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

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionSettingsStore",
                                  "resource://gre/modules/ExtensionSettingsStore.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");

XPCOMUtils.defineLazyGetter(this, "defaultPreferences", function() {
  return new Preferences({defaultBranch: true});
});

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
 * Takes an object of preferenceName:value pairs and either sets or resets the
 * preference to the value.
 *
 * @param {Object} prefsObject
 *        An object with one property per preference, which holds the value to
 *        store in that preference. If the value is undefined then the
 *        preference is reset.
 */
function setPrefs(prefsObject) {
  for (let pref in prefsObject) {
    if (prefsObject[pref] === undefined) {
      Preferences.reset(pref);
    } else {
      Preferences.set(pref, prefsObject[pref]);
    }
  }
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
    let item = await ExtensionSettingsStore.addSetting(
      extension, STORE_TYPE, name, value, initialValueCallback.bind(setting));
    if (item) {
      let prefs = await setting.setCallback(item.value);
      setPrefs(prefs);
      return true;
    }
    return false;
  },

  /**
   * Indicates that this extension no longer wants to set the given preference.
   *
   * @param {Extension} extension
   *        The extension for which a preference setting is being removed.
   * @param {string} name
   *        The unique id of the setting.
   */
  async unsetSetting(extension, name) {
    let item = await ExtensionSettingsStore.removeSetting(
      extension, STORE_TYPE, name);
    if (item) {
      let prefs = item.initialValue || await settingsMap.get(name).setCallback(item.value);
      setPrefs(prefs);
    }
  },

  /**
   * Unsets all previously set settings for an extension. This can be called when
   * an extension is being uninstalled or disabled, for example.
   *
   * @param {Extension} extension The extension for which all settings are being unset.
   */
  async unsetAll(extension) {
    let settings = await ExtensionSettingsStore.getAllForExtension(extension, STORE_TYPE);
    for (let name of settings) {
      await this.unsetSetting(extension, name);
    }
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
    return await ExtensionSettingsStore.getLevelOfControl(extension, STORE_TYPE, name);
  },
};
