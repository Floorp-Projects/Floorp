/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Preference Experiments temporarily change a preference to one of several test
 * values for the duration of the experiment. Telemetry packets are annotated to
 * show what experiments are active, and we use this data to measure the
 * effectiveness of the preference change.
 *
 * Info on active and past experiments is stored in a JSON file in the profile
 * folder.
 *
 * Active preference experiments are stopped if they aren't active on the recipe
 * server. They also expire if Firefox isn't able to contact the recipe server
 * after a period of time, as well as if the user modifies the preference during
 * an active experiment.
 */

/**
 * Experiments store info about an active or expired preference experiment.
 * They are single-depth objects to simplify cloning.
 * @typedef {Object} Experiment
 * @property {string} name
 *   Unique name of the experiment
 * @property {string} branch
 *   Experiment branch that the user was matched to
 * @property {boolean} expired
 *   If false, the experiment is active.
 * @property {string} lastSeen
 *   ISO-formatted date string of when the experiment was last seen from the
 *   recipe server.
 * @property {string} preferenceName
 *   Name of the preference affected by this experiment.
 * @property {string|integer|boolean} preferenceValue
 *   Value to change the preference to during the experiment.
 * @property {string} preferenceType
 *   Type of the preference value being set.
 * @property {string|integer|boolean|undefined} previousPreferenceValue
 *   Value of the preference prior to the experiment, or undefined if it was
 *   unset.
 * @property {PreferenceBranchType} preferenceBranchType
 *   Controls how we modify the preference to affect the client.
 *
 *   If "default", when the experiment is active, the default value for the
 *   preference is modified on startup of the add-on. If "user", the user value
 *   for the preference is modified when the experiment starts, and is reset to
 *   its original value when the experiment ends.
 * @property {string} experimentType
 *   The type to report to Telemetry's experiment marker API.
 */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "CleanupManager", "resource://normandy/lib/CleanupManager.jsm");
ChromeUtils.defineModuleGetter(this, "JSONFile", "resource://gre/modules/JSONFile.jsm");
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
ChromeUtils.defineModuleGetter(this, "LogManager", "resource://normandy/lib/LogManager.jsm");
ChromeUtils.defineModuleGetter(this, "TelemetryEnvironment", "resource://gre/modules/TelemetryEnvironment.jsm");
ChromeUtils.defineModuleGetter(this, "TelemetryEvents", "resource://normandy/lib/TelemetryEvents.jsm");

var EXPORTED_SYMBOLS = ["PreferenceExperiments"];

const EXPERIMENT_FILE = "shield-preference-experiments.json";
const STARTUP_EXPERIMENT_PREFS_BRANCH = "app.normandy.startupExperimentPrefs.";

const MAX_EXPERIMENT_TYPE_LENGTH = 20; // enforced by TelemetryEnvironment
const EXPERIMENT_TYPE_PREFIX = "normandy-";
const MAX_EXPERIMENT_SUBTYPE_LENGTH = MAX_EXPERIMENT_TYPE_LENGTH - EXPERIMENT_TYPE_PREFIX.length;

const PREFERENCE_TYPE_MAP = {
  boolean: Services.prefs.PREF_BOOL,
  string: Services.prefs.PREF_STRING,
  integer: Services.prefs.PREF_INT,
};

const UserPreferences = Services.prefs;
const DefaultPreferences = Services.prefs.getDefaultBranch("");

/**
 * Enum storing Preference modules for each type of preference branch.
 * @enum {Object}
 */
const PreferenceBranchType = {
  user: UserPreferences,
  default: DefaultPreferences,
};

/**
 * Asynchronously load the JSON file that stores experiment status in the profile.
 */
let storePromise;
function ensureStorage() {
  if (storePromise === undefined) {
    const path = OS.Path.join(OS.Constants.Path.profileDir, EXPERIMENT_FILE);
    const storage = new JSONFile({path});
    storePromise = storage.load().then(() => storage);
  }
  return storePromise;
}

const log = LogManager.getLogger("preference-experiments");

// List of active preference observers. Cleaned up on shutdown.
let experimentObservers = new Map();
CleanupManager.addCleanupHandler(() => PreferenceExperiments.stopAllObservers());

function getPref(prefBranch, prefName, prefType) {
  if (prefBranch.getPrefType(prefName) === 0) {
    // pref doesn't exist
    return null;
  }

  switch (prefType) {
    case "boolean": {
      return prefBranch.getBoolPref(prefName);
    }

    case "string":
      return prefBranch.getStringPref(prefName);

    case "integer":
      return prefBranch.getIntPref(prefName);

    default:
      throw new TypeError(`Unexpected preference type (${prefType}) for ${prefName}.`);
  }
}

function setPref(prefBranch, prefName, prefType, prefValue) {
  switch (prefType) {
    case "boolean":
      prefBranch.setBoolPref(prefName, prefValue);
      break;

    case "string":
      prefBranch.setStringPref(prefName, prefValue);
      break;

    case "integer":
      prefBranch.setIntPref(prefName, prefValue);
      break;

    default:
      throw new TypeError(`Unexpected preference type (${prefType}) for ${prefName}.`);
  }
}

var PreferenceExperiments = {
  /**
   * Update the the experiment storage with changes that happened during early startup.
   * @param {object} studyPrefsChanged Map from pref name to previous pref value
   */
  async recordOriginalValues(studyPrefsChanged) {
    const store = await ensureStorage();

    for (const experiment of Object.values(store.data)) {
      if (studyPrefsChanged.hasOwnProperty(experiment.preferenceName)) {
        if (experiment.expired) {
          log.warn("Expired preference experiment changed value during startup");
        }
        if (experiment.branch !== "default") {
          log.warn("Non-default branch preference experiment changed value during startup");
        }
        experiment.previousPreferenceValue = studyPrefsChanged[experiment.preferenceName];
      }
    }

    // not calling store.saveSoon() because if the data doesn't get
    // written, it will get updated with fresher data next time the
    // browser starts.
  },

  /**
   * Set the default preference value for active experiments that use the
   * default preference branch.
   */
  async init() {
    CleanupManager.addCleanupHandler(this.saveStartupPrefs.bind(this));

    for (const experiment of await this.getAllActive()) {
      // Check that the current value of the preference is still what we set it to
      if (getPref(UserPreferences, experiment.preferenceName, experiment.preferenceType) !== experiment.preferenceValue) {
        // if not, stop the experiment, and skip the remaining steps
        log.info(`Stopping experiment "${experiment.name}" because its value changed`);
        await this.stop(experiment.name, {
          resetValue: false,
          reason: "user-preference-changed-sideload",
        });
        continue;
      }

      // Notify Telemetry of experiments we're running, since they don't persist between restarts
      TelemetryEnvironment.setExperimentActive(
        experiment.name,
        experiment.branch,
        {type: EXPERIMENT_TYPE_PREFIX + experiment.experimentType}
      );

      // Watch for changes to the experiment's preference
      this.startObserver(experiment.name, experiment.preferenceName, experiment.preferenceType, experiment.preferenceValue);
    }
  },

  /**
   * Save in-progress preference experiments in a sub-branch of the shield
   * prefs. On startup, we read these to set the experimental values.
   */
  async saveStartupPrefs() {
    const prefBranch = Services.prefs.getBranch(STARTUP_EXPERIMENT_PREFS_BRANCH);
    prefBranch.deleteBranch("");

    for (const experiment of await this.getAllActive()) {
      const name = experiment.preferenceName;
      const value = experiment.preferenceValue;

      switch (typeof value) {
        case "string":
          prefBranch.setCharPref(name, value);
          break;

        case "number":
          prefBranch.setIntPref(name, value);
          break;

        case "boolean":
          prefBranch.setBoolPref(name, value);
          break;

        default:
          throw new Error(`Invalid preference type ${typeof value}`);
      }
    }
  },

  /**
   * Test wrapper that temporarily replaces the stored experiment data with fake
   * data for testing.
   */
  withMockExperiments(testFunction) {
    return async function inner(...args) {
      const oldPromise = storePromise;
      const mockExperiments = {};
      storePromise = Promise.resolve({
        data: mockExperiments,
        saveSoon() { },
      });
      const oldObservers = experimentObservers;
      experimentObservers = new Map();
      try {
        await testFunction(...args, mockExperiments);
      } finally {
        storePromise = oldPromise;
        PreferenceExperiments.stopAllObservers();
        experimentObservers = oldObservers;
      }
    };
  },

  /**
   * Clear all stored data about active and past experiments.
   */
  async clearAllExperimentStorage() {
    const store = await ensureStorage();
    store.data = {};
    store.saveSoon();
  },

  /**
   * Start a new preference experiment.
   * @param {Object} experiment
   * @param {string} experiment.name
   * @param {string} experiment.branch
   * @param {string} experiment.preferenceName
   * @param {string|integer|boolean} experiment.preferenceValue
   * @param {PreferenceBranchType} experiment.preferenceBranchType
   * @rejects {Error}
   *   - If an experiment with the given name already exists
   *   - if an experiment for the given preference is active
   *   - If the given preferenceType does not match the existing stored preference
   */
  async start({
    name,
    branch,
    preferenceName,
    preferenceValue,
    preferenceBranchType,
    preferenceType,
    experimentType = "exp",
  }) {
    log.debug(`PreferenceExperiments.start(${name}, ${branch})`);

    const store = await ensureStorage();
    if (name in store.data) {
      TelemetryEvents.sendEvent("enrollFailed", "preference_study", name, {reason: "name-conflict"});
      throw new Error(`A preference experiment named "${name}" already exists.`);
    }

    const activeExperiments = Object.values(store.data).filter(e => !e.expired);
    const hasConflictingExperiment = activeExperiments.some(
      e => e.preferenceName === preferenceName
    );
    if (hasConflictingExperiment) {
      TelemetryEvents.sendEvent("enrollFailed", "preference_study", name, {reason: "pref-conflict"});
      throw new Error(
        `Another preference experiment for the pref "${preferenceName}" is currently active.`
      );
    }

    const preferences = PreferenceBranchType[preferenceBranchType];
    if (!preferences) {
      TelemetryEvents.sendEvent("enrollFailed", "preference_study", name, {reason: "invalid-branch"});
      throw new Error(`Invalid value for preferenceBranchType: ${preferenceBranchType}`);
    }

    if (experimentType.length > MAX_EXPERIMENT_SUBTYPE_LENGTH) {
      TelemetryEvents.sendEvent("enrollFailed", "preference_study", name, {reason: "experiment-type-too-long"});
      throw new Error(
        `experimentType must be less than ${MAX_EXPERIMENT_SUBTYPE_LENGTH} characters. ` +
        `"${experimentType}" is ${experimentType.length} long.`
      );
    }

    const prevPrefType = Services.prefs.getPrefType(preferenceName);
    const givenPrefType = PREFERENCE_TYPE_MAP[preferenceType];

    if (!preferenceType || !givenPrefType) {
      TelemetryEvents.sendEvent("enrollFailed", "preference_study", name, {reason: "invalid-type"});
      throw new Error(`Invalid preferenceType provided (given "${preferenceType}")`);
    }

    if (prevPrefType !== Services.prefs.PREF_INVALID && prevPrefType !== givenPrefType) {
      TelemetryEvents.sendEvent("enrollFailed", "preference_study", name, {reason: "invalid-type"});
      throw new Error(
        `Previous preference value is of type "${prevPrefType}", but was given ` +
        `"${givenPrefType}" (${preferenceType})`
      );
    }

    /** @type {Experiment} */
    const experiment = {
      name,
      branch,
      expired: false,
      lastSeen: new Date().toJSON(),
      preferenceName,
      preferenceValue,
      preferenceType,
      previousPreferenceValue: getPref(preferences, preferenceName, preferenceType),
      preferenceBranchType,
      experimentType,
    };

    setPref(preferences, preferenceName, preferenceType, preferenceValue);
    PreferenceExperiments.startObserver(name, preferenceName, preferenceType, preferenceValue);
    store.data[name] = experiment;
    store.saveSoon();

    TelemetryEnvironment.setExperimentActive(name, branch, {type: EXPERIMENT_TYPE_PREFIX + experimentType});
    TelemetryEvents.sendEvent("enroll", "preference_study", name, {experimentType, branch});
    await this.saveStartupPrefs();
  },

  /**
   * Register a preference observer that stops an experiment when the user
   * modifies the preference.
   * @param {string} experimentName
   * @param {string} preferenceName
   * @param {string|integer|boolean} preferenceValue
   * @throws {Error}
   *   If an observer for the named experiment is already active.
   */
  startObserver(experimentName, preferenceName, preferenceType, preferenceValue) {
    log.debug(`PreferenceExperiments.startObserver(${experimentName})`);

    if (experimentObservers.has(experimentName)) {
      throw new Error(
        `An observer for the preference experiment ${experimentName} is already active.`
      );
    }

    const observerInfo = {
      preferenceName,
      observer() {
        const newValue = getPref(UserPreferences, preferenceName, preferenceType);
        if (newValue !== preferenceValue) {
          PreferenceExperiments.stop(experimentName, {
            resetValue: false,
            reason: "user-preference-changed",
          }).catch(Cu.reportError);
        }
      },
    };
    experimentObservers.set(experimentName, observerInfo);
    Services.prefs.addObserver(preferenceName, observerInfo.observer);
  },

  /**
   * Check if a preference observer is active for an experiment.
   * @param {string} experimentName
   * @return {Boolean}
   */
  hasObserver(experimentName) {
    log.debug(`PreferenceExperiments.hasObserver(${experimentName})`);
    return experimentObservers.has(experimentName);
  },

  /**
   * Disable a preference observer for the named experiment.
   * @param {string} experimentName
   * @throws {Error}
   *   If there is no active observer for the named experiment.
   */
  stopObserver(experimentName) {
    log.debug(`PreferenceExperiments.stopObserver(${experimentName})`);

    if (!experimentObservers.has(experimentName)) {
      throw new Error(`No observer for the preference experiment ${experimentName} found.`);
    }

    const {preferenceName, observer} = experimentObservers.get(experimentName);
    Services.prefs.removeObserver(preferenceName, observer);
    experimentObservers.delete(experimentName);
  },

  /**
   * Disable all currently-active preference observers for experiments.
   */
  stopAllObservers() {
    log.debug("PreferenceExperiments.stopAllObservers()");
    for (const {preferenceName, observer} of experimentObservers.values()) {
      Services.prefs.removeObserver(preferenceName, observer);
    }
    experimentObservers.clear();
  },

  /**
   * Update the timestamp storing when Normandy last sent a recipe for the named
   * experiment.
   * @param {string} experimentName
   * @rejects {Error}
   *   If there is no stored experiment with the given name.
   */
  async markLastSeen(experimentName) {
    log.debug(`PreferenceExperiments.markLastSeen(${experimentName})`);

    const store = await ensureStorage();
    if (!(experimentName in store.data)) {
      throw new Error(`Could not find a preference experiment named "${experimentName}"`);
    }

    store.data[experimentName].lastSeen = new Date().toJSON();
    store.saveSoon();
  },

  /**
   * Stop an active experiment, deactivate preference watchers, and optionally
   * reset the associated preference to its previous value.
   * @param {string} experimentName
   * @param {Object} options
   * @param {boolean} [options.resetValue = true]
   *   If true, reset the preference to its original value prior to
   *   the experiment. Optional, defauls to true.
   * @param {String} [options.reason = "unknown"]
   *   Reason that the experiment is ending. Optional, defaults to
   *   "unknown".
   * @rejects {Error}
   *   If there is no stored experiment with the given name, or if the
   *   experiment has already expired.
   */
  async stop(experimentName, {resetValue = true, reason = "unknown"} = {}) {
    log.debug(`PreferenceExperiments.stop(${experimentName}, {resetValue: ${resetValue}, reason: ${reason}})`);
    if (reason === "unknown") {
      log.warn(`experiment ${experimentName} ending for unknown reason`);
    }

    const store = await ensureStorage();
    if (!(experimentName in store.data)) {
      TelemetryEvents.sendEvent("unenrollFailed", "preference_study", experimentName, {reason: "does-not-exist"});
      throw new Error(`Could not find a preference experiment named "${experimentName}"`);
    }

    const experiment = store.data[experimentName];
    if (experiment.expired) {
      TelemetryEvents.sendEvent("unenrollFailed", "preference_study", experimentName, {reason: "already-unenrolled"});
      throw new Error(
        `Cannot stop preference experiment "${experimentName}" because it is already expired`
      );
    }

    if (PreferenceExperiments.hasObserver(experimentName)) {
      PreferenceExperiments.stopObserver(experimentName);
    }

    if (resetValue) {
      const {preferenceName, preferenceType, previousPreferenceValue, preferenceBranchType} = experiment;
      const preferences = PreferenceBranchType[preferenceBranchType];

      if (previousPreferenceValue !== null) {
        setPref(preferences, preferenceName, preferenceType, previousPreferenceValue);
      } else if (preferenceBranchType === "user") {
        // Remove the "user set" value (which Shield set), but leave the default intact.
        preferences.clearUserPref(preferenceName);
      } else {
        // Remove both the user and default branch preference. This
        // is ok because we only do this when studies expire, not
        // when users actively leave a study by changing the
        // preference, so there should not be a user branch value at
        // this point.
        Services.prefs.getDefaultBranch("").deleteBranch(preferenceName);
      }
    }

    experiment.expired = true;
    store.saveSoon();

    TelemetryEnvironment.setExperimentInactive(experimentName);
    TelemetryEvents.sendEvent("unenroll", "preference_study", experimentName, {
      didResetValue: resetValue ? "true" : "false",
      branch: experiment.branch,
      reason,
    });
    await this.saveStartupPrefs();
  },

  /**
   * Get the experiment object for the named experiment.
   * @param {string} experimentName
   * @resolves {Experiment}
   * @rejects {Error}
   *   If no preference experiment exists with the given name.
   */
  async get(experimentName) {
    log.debug(`PreferenceExperiments.get(${experimentName})`);
    const store = await ensureStorage();
    if (!(experimentName in store.data)) {
      throw new Error(`Could not find a preference experiment named "${experimentName}"`);
    }

    // Return a copy so mutating it doesn't affect the storage.
    return Object.assign({}, store.data[experimentName]);
  },

  /**
   * Get a list of all stored experiment objects.
   * @resolves {Experiment[]}
   */
  async getAll() {
    const store = await ensureStorage();

    // Return copies so that mutating returned experiments doesn't affect the
    // stored values.
    return Object.values(store.data).map(experiment => Object.assign({}, experiment));
  },

  /**
  * Get a list of experiment objects for all active experiments.
  * @resolves {Experiment[]}
  */
  async getAllActive() {
    const store = await ensureStorage();
    // Return copies so mutating them doesn't affect the storage.
    return Object.values(store.data).filter(e => !e.expired).map(e => Object.assign({}, e));
  },

  /**
   * Check if an experiment exists with the given name.
   * @param {string} experimentName
   * @resolves {boolean} True if the experiment exists, false if it doesn't.
   */
  async has(experimentName) {
    log.debug(`PreferenceExperiments.has(${experimentName})`);
    const store = await ensureStorage();
    return experimentName in store.data;
  },
};
