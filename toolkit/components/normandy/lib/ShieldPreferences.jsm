/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(
  this, "AddonStudies", "resource://normandy/lib/AddonStudies.jsm"
);
ChromeUtils.defineModuleGetter(
  this, "CleanupManager", "resource://normandy/lib/CleanupManager.jsm"
);

var EXPORTED_SYMBOLS = ["ShieldPreferences"];

const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed"; // from modules/libpref/nsIPrefBranch.idl
const PREF_OPT_OUT_STUDIES_ENABLED = "app.shield.optoutstudies.enabled";

/**
 * Handles Shield-specific preferences, including their UI.
 */
var ShieldPreferences = {
  init() {
    // Watch for changes to the Opt-out pref
    Services.prefs.addObserver(PREF_OPT_OUT_STUDIES_ENABLED, this);
    CleanupManager.addCleanupHandler(() => {
      Services.prefs.removeObserver(PREF_OPT_OUT_STUDIES_ENABLED, this);
    });
  },

  observe(subject, topic, data) {
    switch (topic) {
      case NS_PREFBRANCH_PREFCHANGE_TOPIC_ID:
        this.observePrefChange(data);
        break;
    }
  },

  async observePrefChange(prefName) {
    let prefValue;
    switch (prefName) {
      // If the opt-out pref changes to be false, disable all current studies.
      case PREF_OPT_OUT_STUDIES_ENABLED: {
        prefValue = Services.prefs.getBoolPref(PREF_OPT_OUT_STUDIES_ENABLED);
        if (!prefValue) {
          for (const study of await AddonStudies.getAll()) {
            if (study.active) {
              await AddonStudies.stop(study.recipeId, "general-opt-out");
            }
          }
        }
        break;
      }
    }
  },
};
