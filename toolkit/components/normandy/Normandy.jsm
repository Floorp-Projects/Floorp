/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/Log.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AboutPages: "resource://normandy-content/AboutPages.jsm",
  AddonStudies: "resource://normandy/lib/AddonStudies.jsm",
  CleanupManager: "resource://normandy/lib/CleanupManager.jsm",
  LogManager: "resource://normandy/lib/LogManager.jsm",
  PreferenceExperiments: "resource://normandy/lib/PreferenceExperiments.jsm",
  RecipeRunner: "resource://normandy/lib/RecipeRunner.jsm",
  ShieldPreferences: "resource://normandy/lib/ShieldPreferences.jsm",
  TelemetryEvents: "resource://normandy/lib/TelemetryEvents.jsm",
});

var EXPORTED_SYMBOLS = ["Normandy"];

const UI_AVAILABLE_NOTIFICATION = "sessionstore-windows-restored";
const BOOTSTRAP_LOGGER_NAME = "app.normandy.bootstrap";
const SHIELD_INIT_NOTIFICATION = "shield-init-complete";

const PREF_PREFIX = "app.normandy";
const LEGACY_PREF_PREFIX = "extensions.shield-recipe-client";
const STARTUP_EXPERIMENT_PREFS_BRANCH = `${PREF_PREFIX}.startupExperimentPrefs.`;
const PREF_LOGGING_LEVEL = `${PREF_PREFIX}.logging.level`;

// Logging
const log = Log.repository.getLogger(BOOTSTRAP_LOGGER_NAME);
log.addAppender(new Log.ConsoleAppender(new Log.BasicFormatter()));
log.level = Services.prefs.getIntPref(PREF_LOGGING_LEVEL, Log.Level.Warn);

let studyPrefsChanged = {};

var Normandy = {
  init() {
    // Initialization that needs to happen before the first paint on startup.
    this.migrateShieldPrefs();
    this.initExperimentPrefs();

    // Wait until the UI is available before finishing initialization.
    Services.obs.addObserver(this, UI_AVAILABLE_NOTIFICATION);
  },

  observe(subject, topic, data) {
    if (topic === UI_AVAILABLE_NOTIFICATION) {
      Services.obs.removeObserver(this, UI_AVAILABLE_NOTIFICATION);
      this.finishInit();
    }
  },

  async finishInit() {
    await PreferenceExperiments.recordOriginalValues(studyPrefsChanged);

    // Setup logging and listen for changes to logging prefs
    LogManager.configure(Services.prefs.getIntPref(PREF_LOGGING_LEVEL, Log.Level.Warn));
    Services.prefs.addObserver(PREF_LOGGING_LEVEL, LogManager.configure);
    CleanupManager.addCleanupHandler(
      () => Services.prefs.removeObserver(PREF_LOGGING_LEVEL, LogManager.configure),
    );

    try {
      TelemetryEvents.init();
    } catch (err) {
      log.error("Failed to initialize telemetry events:", err);
    }

    try {
      await AboutPages.init();
    } catch (err) {
      log.error("Failed to initialize about pages:", err);
    }

    try {
      await AddonStudies.init();
    } catch (err) {
      log.error("Failed to initialize addon studies:", err);
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
    Services.prefs.removeObserver(PREF_LOGGING_LEVEL, LogManager.configure);

    // In case the observer didn't run, clean it up.
    try {
      Services.obs.removeObserver(this, UI_AVAILABLE_NOTIFICATION);
    } catch (err) {
      // It must already be removed!
    }
  },

  migrateShieldPrefs() {
    const legacyBranch = Services.prefs.getBranch(LEGACY_PREF_PREFIX + ".");
    const newBranch = Services.prefs.getBranch(PREF_PREFIX + ".");

    for (const prefName of legacyBranch.getChildList("")) {
      const legacyPrefType = legacyBranch.getPrefType(prefName);
      const newPrefType = newBranch.getPrefType(prefName);

      // If new preference exists and is not the same as the legacy pref, skip it
      if (newPrefType !== Services.prefs.PREF_INVALID && newPrefType !== legacyPrefType) {
        log.error(`Error migrating normandy pref ${prefName}; pref type does not match.`);
        continue;
      }

      // Now move the value over. If it matches the default, this will be a no-op
      switch (legacyPrefType) {
        case Services.prefs.PREF_STRING:
          newBranch.setCharPref(prefName, legacyBranch.getCharPref(prefName));
          break;

        case Services.prefs.PREF_INT:
          newBranch.setIntPref(prefName, legacyBranch.getIntPref(prefName));
          break;

        case Services.prefs.PREF_BOOL:
          newBranch.setBoolPref(prefName, legacyBranch.getBoolPref(prefName));
          break;

        case Services.prefs.PREF_INVALID:
          // This should never happen.
          log.error(`Error migrating pref ${prefName}; pref type is invalid (${legacyPrefType}).`);
          break;

        default:
          // This should never happen either.
          log.error(`Error getting startup pref ${prefName}; unknown value type ${legacyPrefType}.`);
      }

      legacyBranch.clearUserPref(prefName);
    }
  },

  initExperimentPrefs() {
    studyPrefsChanged = {};
    const defaultBranch = Services.prefs.getDefaultBranch("");
    const experimentBranch = Services.prefs.getBranch(STARTUP_EXPERIMENT_PREFS_BRANCH);

    for (const prefName of experimentBranch.getChildList("")) {
      const experimentPrefType = experimentBranch.getPrefType(prefName);
      const realPrefType = defaultBranch.getPrefType(prefName);

      if (realPrefType !== Services.prefs.PREF_INVALID && realPrefType !== experimentPrefType) {
        log.error(`Error setting startup pref ${prefName}; pref type does not match.`);
        continue;
      }

      // record the value of the default branch before setting it
      try {
        switch (realPrefType) {
          case Services.prefs.PREF_STRING:
            studyPrefsChanged[prefName] = defaultBranch.getCharPref(prefName);
            break;

          case Services.prefs.PREF_INT:
            studyPrefsChanged[prefName] = defaultBranch.getIntPref(prefName);
            break;

          case Services.prefs.PREF_BOOL:
            studyPrefsChanged[prefName] = defaultBranch.getBoolPref(prefName);
            break;

          case Services.prefs.PREF_INVALID:
            studyPrefsChanged[prefName] = null;
            break;

          default:
            // This should never happen
            log.error(`Error getting startup pref ${prefName}; unknown value type ${experimentPrefType}.`);
        }
      } catch (e) {
        if (e.result === Cr.NS_ERROR_UNEXPECTED) {
          // There is a value for the pref on the user branch but not on the default branch. This is ok.
          studyPrefsChanged[prefName] = null;
        } else {
          // rethrow
          throw e;
        }
      }

      // now set the new default value
      switch (experimentPrefType) {
        case Services.prefs.PREF_STRING:
          defaultBranch.setCharPref(prefName, experimentBranch.getCharPref(prefName));
          break;

        case Services.prefs.PREF_INT:
          defaultBranch.setIntPref(prefName, experimentBranch.getIntPref(prefName));
          break;

        case Services.prefs.PREF_BOOL:
          defaultBranch.setBoolPref(prefName, experimentBranch.getBoolPref(prefName));
          break;

        case Services.prefs.PREF_INVALID:
          // This should never happen.
          log.error(`Error setting startup pref ${prefName}; pref type is invalid (${experimentPrefType}).`);
          break;

        default:
          // This should never happen either.
          log.error(`Error getting startup pref ${prefName}; unknown value type ${experimentPrefType}.`);
      }
    }
  },
};
