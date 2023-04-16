/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BranchedAddonStudyAction:
    "resource://normandy/actions/BranchedAddonStudyAction.sys.mjs",
  AddonStudies: "resource://normandy/lib/AddonStudies.sys.mjs",
  CleanupManager: "resource://normandy/lib/CleanupManager.sys.mjs",
  PreferenceExperiments:
    "resource://normandy/lib/PreferenceExperiments.sys.mjs",
});

const NS_PREFBRANCH_PREFCHANGE_TOPIC_ID = "nsPref:changed"; // from modules/libpref/nsIPrefBranch.idl
const PREF_OPT_OUT_STUDIES_ENABLED = "app.shield.optoutstudies.enabled";

/**
 * Handles Shield-specific preferences, including their UI.
 */
export var ShieldPreferences = {
  init() {
    // Watch for changes to the Opt-out pref
    Services.prefs.addObserver(PREF_OPT_OUT_STUDIES_ENABLED, this);

    lazy.CleanupManager.addCleanupHandler(() => {
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
          const action = new lazy.BranchedAddonStudyAction();
          const studyPromises = (await lazy.AddonStudies.getAll()).map(
            study => {
              if (!study.active) {
                return null;
              }
              return action.unenroll(study.recipeId, "general-opt-out");
            }
          );

          const experimentPromises = (
            await lazy.PreferenceExperiments.getAll()
          ).map(experiment => {
            if (experiment.expired) {
              return null;
            }
            return lazy.PreferenceExperiments.stop(experiment.slug, {
              reason: "general-opt-out",
              caller: "observePrefChange::general-opt-out",
            });
          });

          const allPromises = studyPromises
            .concat(experimentPromises)
            .map(p => p && p.catch(err => console.error(err)));
          await Promise.all(allPromises);
        }
        break;
      }
    }
  },
};
