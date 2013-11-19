/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

this.EXPORTED_SYMBOLS = [];

#ifdef XP_WIN
#ifdef MOZ_METRO

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu, manager: Cm} =
  Components;
const PREF_BASE_KEY = "Software\\Mozilla\\Firefox\\Metro\\Prefs\\";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = [ "WindowsPrefSync" ];

/**
 * Manages preferences that need to be pulled and pushed between Metro
 * and desktop.
 */
this.WindowsPrefSync = {
  init: function() {
    this.pullSharedPrefs();
    this.prefListToPush.forEach(function(prefName) {
      this.pushSharedPref(prefName);
      Services.prefs.addObserver(prefName, this, false);
    }, this);
  },

  uninit: function() {
    this.prefListToPush.forEach(function(prefName) {
        Services.prefs.removeObserver(prefName, this);
    }, this);
  },

  /**
   * Returns the list of prefs that should be pushed for the current
   * environment.
   */
  get prefListToPush() {
    return !Services.metro.immersive ? this.desktopControlledPrefs :
      this.metroControlledPrefs;
  },

  /**
   * Returns the list of prefs that should be pulled for the current
   * environment.
   */
  get prefListToPull() {
    return Services.metro.immersive ? this.desktopControlledPrefs :
      this.metroControlledPrefs;
  },

  /**
   * The following preferences will be pushed to registry from Desktop
   * Firefox and pulled in from Metro Firefox.
   *
   * app.update.* prefs are because Metro shares an installation directory with
   * Firefox, and the options for these are only present in the Desktop options.
   *
   * browser.sessionstore.resume_session_once is mainly for the switch to Metro
   * and switch to Desktop feature.
   */
  desktopControlledPrefs: ["app.update.auto",
    "app.update.enabled",
    "app.update.service.enabled",
    "app.update.metro.enabled",
    "browser.sessionstore.resume_session_once"],

  /**
   * The following preferences will be pushed to registry from Metro
   * Firefox and pulled in from Desktop Firefox.
   *
   * browser.sessionstore.resume_session_once is mainly for the switch to Metro
   * and switch to Desktop feature.
   */
  metroControlledPrefs: ["browser.sessionstore.resume_session_once"],

  /**
   * Observes preference changes and writes them to the registry, only
   * the list of preferences initialized will be observed
   */
  observe: function (aSubject, aTopic, aPrefName) {
    if (aTopic != "nsPref:changed")
      return;

    this.pushSharedPref(aPrefName);
  },

  /**
   * Writes the pref to HKCU in the registry and adds a pref-observer to keep
   * the registry in sync with changes to the value.
   */
  pushSharedPref : function(aPrefName) {
    let registry = Cc["@mozilla.org/windows-registry-key;1"].
      createInstance(Ci.nsIWindowsRegKey);
    try {
      var prefType = Services.prefs.getPrefType(aPrefName);
      let prefFunc;
      if (prefType == Ci.nsIPrefBranch.PREF_INT)
        prefFunc = "getIntPref";
      else if (prefType == Ci.nsIPrefBranch.PREF_BOOL)
        prefFunc = "getBoolPref";
      else if (prefType == Ci.nsIPrefBranch.PREF_STRING)
        prefFunc = "getCharPref";
      else
        throw "Unsupported pref type";

      let prefValue = Services.prefs[prefFunc](aPrefName);
      registry.create(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
        PREF_BASE_KEY + prefType,
        Ci.nsIWindowsRegKey.ACCESS_WRITE);
      // Always write as string, but the registry subfolder will determine
      // how Metro interprets that string value.
      registry.writeStringValue(aPrefName, prefValue);
    } catch (ex) {
      Cu.reportError("Couldn't push pref " + aPrefName + ": " + ex);
    } finally {
      registry.close();
    }
  },

  /**
   * Pulls in all shared prefs from the registry
   */
  pullSharedPrefs: function() {
    function pullSharedPrefType(prefType, prefFunc) {
      try {
        registry.create(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
          PREF_BASE_KEY + prefType,
          Ci.nsIWindowsRegKey.ACCESS_ALL);
        for (let i = 0; i < registry.valueCount; i++) {
          let prefName = registry.getValueName(i);
          let prefValue = registry.readStringValue(prefName);
          if (prefType == Ci.nsIPrefBranch.PREF_BOOL) {
            prefValue = prefValue == "true";
          }
          if (self.prefListToPull.indexOf(prefName) != -1) {
            Services.prefs[prefFunc](prefName, prefValue);
          }
        }
      } catch (ex) {
        dump("Could not pull for prefType " + prefType + ": " + ex + "\n");
      } finally {
        registry.close();
      }
    }
    let self = this;
    let registry = Cc["@mozilla.org/windows-registry-key;1"].
      createInstance(Ci.nsIWindowsRegKey);
    pullSharedPrefType(Ci.nsIPrefBranch.PREF_INT, "setIntPref");
    pullSharedPrefType(Ci.nsIPrefBranch.PREF_BOOL, "setBoolPref");
    pullSharedPrefType(Ci.nsIPrefBranch.PREF_STRING, "setCharPref");
  }
};
#endif
#endif
