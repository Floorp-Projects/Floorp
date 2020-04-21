/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonRollouts: "resource://normandy/lib/AddonRollouts.jsm",
  AddonStudies: "resource://normandy/lib/AddonStudies.jsm",
  CleanupManager: "resource://normandy/lib/CleanupManager.jsm",
  LogManager: "resource://normandy/lib/LogManager.jsm",
  NormandyMigrations: "resource://normandy/NormandyMigrations.jsm",
  PreferenceExperiments: "resource://normandy/lib/PreferenceExperiments.jsm",
  PreferenceRollouts: "resource://normandy/lib/PreferenceRollouts.jsm",
  RecipeRunner: "resource://normandy/lib/RecipeRunner.jsm",
  ShieldPreferences: "resource://normandy/lib/ShieldPreferences.jsm",
  TelemetryUtils: "resource://gre/modules/TelemetryUtils.jsm",
  TelemetryEvents: "resource://normandy/lib/TelemetryEvents.jsm",
});

var EXPORTED_SYMBOLS = ["Normandy"];

const UI_AVAILABLE_NOTIFICATION = "sessionstore-windows-restored";
const BOOTSTRAP_LOGGER_NAME = "app.normandy.bootstrap";
const SHIELD_INIT_NOTIFICATION = "shield-init-complete";

const STARTUP_EXPERIMENT_PREFS_BRANCH = "app.normandy.startupExperimentPrefs.";
const STARTUP_ROLLOUT_PREFS_BRANCH = "app.normandy.startupRolloutPrefs.";
const PREF_LOGGING_LEVEL = "app.normandy.logging.level";

// Logging
const log = Log.repository.getLogger(BOOTSTRAP_LOGGER_NAME);
log.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));
log.level = Services.prefs.getIntPref(PREF_LOGGING_LEVEL, Log.Level.Warn);

var Normandy = {
  studyPrefsChanged: {},
  rolloutPrefsChanged: {},

  async init({ runAsync = true } = {}) {
    // Initialization that needs to happen before the first paint on startup.
    Services.obs.addObserver(
      this,
      TelemetryUtils.TELEMETRY_UPLOAD_DISABLED_TOPIC
    );

    await NormandyMigrations.applyAll();
    this.rolloutPrefsChanged = this.applyStartupPrefs(
      STARTUP_ROLLOUT_PREFS_BRANCH
    );
    this.studyPrefsChanged = this.applyStartupPrefs(
      STARTUP_EXPERIMENT_PREFS_BRANCH
    );

    if (runAsync) {
      Services.obs.addObserver(this, UI_AVAILABLE_NOTIFICATION);
    } else {
      // Remove any observers, if present.
      try {
        Services.obs.removeObserver(this, UI_AVAILABLE_NOTIFICATION);
      } catch (e) {}

      await this.finishInit();
    }
  },

  async observe(subject, topic, data) {
    if (topic === UI_AVAILABLE_NOTIFICATION) {
      Services.obs.removeObserver(this, UI_AVAILABLE_NOTIFICATION);
      this.finishInit();
    } else if (topic === TelemetryUtils.TELEMETRY_UPLOAD_DISABLED_TOPIC) {
      await Promise.all(
        [
          PreferenceExperiments,
          PreferenceRollouts,
          AddonStudies,
          AddonRollouts,
        ].map(service => service.onTelemetryDisabled())
      );
    }
  },

  async finishInit() {
    try {
      TelemetryEvents.init();
    } catch (err) {
      log.error("Failed to initialize telemetry events:", err);
    }

    await PreferenceRollouts.recordOriginalValues(this.rolloutPrefsChanged);
    await PreferenceExperiments.recordOriginalValues(this.studyPrefsChanged);

    // Setup logging and listen for changes to logging prefs
    LogManager.configure(
      Services.prefs.getIntPref(PREF_LOGGING_LEVEL, Log.Level.Warn)
    );
    Services.prefs.addObserver(PREF_LOGGING_LEVEL, LogManager.configure);
    CleanupManager.addCleanupHandler(() =>
      Services.prefs.removeObserver(PREF_LOGGING_LEVEL, LogManager.configure)
    );

    try {
      await AddonStudies.init();
    } catch (err) {
      log.error("Failed to initialize addon studies:", err);
    }

    try {
      await PreferenceRollouts.init();
    } catch (err) {
      log.error("Failed to initialize preference rollouts:", err);
    }

    try {
      await AddonRollouts.init();
    } catch (err) {
      log.error("Failed to initialize addon rollouts:", err);
    }

    try {
      await PreferenceExperiments.init();
    } catch (err) {
      log.error("Failed to initialize preference experiments:", err);
    }

    try {
      ShieldPreferences.init();
    } catch (err) {
      log.error("Failed to initialize preferences UI:", err);
    }

    await RecipeRunner.init();
    Services.obs.notifyObservers(null, SHIELD_INIT_NOTIFICATION);
  },

  async uninit() {
    await CleanupManager.cleanup();
    // Note that Service.pref.removeObserver and Service.obs.removeObserver have
    // oppositely ordered parameters.
    Services.prefs.removeObserver(PREF_LOGGING_LEVEL, LogManager.configure);
    for (const topic of [
      TelemetryUtils.TELEMETRY_UPLOAD_DISABLED_TOPIC,
      UI_AVAILABLE_NOTIFICATION,
    ]) {
      try {
        Services.obs.removeObserver(this, topic);
      } catch (e) {
        // topic must have already been removed or never added
      }
    }
    await PreferenceRollouts.uninit();
  },

  /**
   * Copy a preference subtree from one branch to another, being careful about
   * types, and return the values the target branch originally had. Prefs will
   * be read from the user branch and applied to the default branch.
   * @param sourcePrefix
   *   The pref prefix to read prefs from.
   * @returns
   *   The original values that each pref had on the default branch.
   */
  applyStartupPrefs(sourcePrefix) {
    const originalValues = {};
    const sourceBranch = Services.prefs.getBranch(sourcePrefix);
    const targetBranch = Services.prefs.getDefaultBranch("");

    for (const prefName of sourceBranch.getChildList("")) {
      const sourcePrefType = sourceBranch.getPrefType(prefName);
      const targetPrefType = targetBranch.getPrefType(prefName);

      if (
        targetPrefType !== Services.prefs.PREF_INVALID &&
        targetPrefType !== sourcePrefType
      ) {
        Cu.reportError(
          new Error(
            `Error setting startup pref ${prefName}; pref type does not match.`
          )
        );
        continue;
      }

      // record the value of the default branch before setting it
      try {
        switch (targetPrefType) {
          case Services.prefs.PREF_STRING: {
            originalValues[prefName] = targetBranch.getCharPref(prefName);
            break;
          }
          case Services.prefs.PREF_INT: {
            originalValues[prefName] = targetBranch.getIntPref(prefName);
            break;
          }
          case Services.prefs.PREF_BOOL: {
            originalValues[prefName] = targetBranch.getBoolPref(prefName);
            break;
          }
          case Services.prefs.PREF_INVALID: {
            originalValues[prefName] = null;
            break;
          }
          default: {
            // This should never happen
            log.error(
              `Error getting startup pref ${prefName}; unknown value type ${sourcePrefType}.`
            );
          }
        }
      } catch (e) {
        if (e.result === Cr.NS_ERROR_UNEXPECTED) {
          // There is a value for the pref on the user branch but not on the default branch. This is ok.
          originalValues[prefName] = null;
        } else {
          // Unexpected error, report it and move on
          Cu.reportError(e);
          continue;
        }
      }

      // now set the new default value
      switch (sourcePrefType) {
        case Services.prefs.PREF_STRING: {
          targetBranch.setCharPref(
            prefName,
            sourceBranch.getCharPref(prefName)
          );
          break;
        }
        case Services.prefs.PREF_INT: {
          targetBranch.setIntPref(prefName, sourceBranch.getIntPref(prefName));
          break;
        }
        case Services.prefs.PREF_BOOL: {
          targetBranch.setBoolPref(
            prefName,
            sourceBranch.getBoolPref(prefName)
          );
          break;
        }
        default: {
          // This should never happen.
          Cu.reportError(
            new Error(
              `Error getting startup pref ${prefName}; unexpected value type ${sourcePrefType}.`
            )
          );
        }
      }
    }

    return originalValues;
  },
};
