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
 * @typedef {Object} Experiment
 * @property {string} slug
 *   A string uniquely identifying the experiment. Used for telemetry, and other
 *   machine-oriented use cases. Used as a display name if `userFacingName` is
 *   null.
 * @property {string|null} userFacingName
 *   A user-friendly name for the experiment. Null on old-style single-preference
 *   experiments, which do not have a userFacingName.
 * @property {string|null} userFacingDescription
 *   A user-friendly description of the experiment. Null on old-style
 *   single-preference experiments, which do not have a userFacingDescription.
 * @property {string} branch
 *   Experiment branch that the user was matched to
 * @property {boolean} expired
 *   If false, the experiment is active.
 *   ISO-formatted date string of when the experiment was last seen from the
 *   recipe server.
 * @property {string|null} temporaryErrorDeadline
 *   ISO-formatted date string of when temporary errors with this experiment
 *   should not longer be considered temporary. After this point, further errors
 *   will result in unenrollment.
 * @property {Object} preferences
 *   An object consisting of all the preferences that are set by this experiment.
 *   Keys are the name of each preference affected by this experiment. Values are
 *   Preference Objects, about which see below.
 * @property {string} experimentType
 *   The type to report to Telemetry's experiment marker API.
 * @property {string} enrollmentId
 *   A random ID generated at time of enrollment. It should be included on all
 *   telemetry related to this experiment. It should not be re-used by other
 *   studies, or any other purpose. May be null on old experiments.
 * @property {string} actionName
 *   The action who knows about this experiment and is responsible for cleaning
 *   it up. This should correspond to the `name` of some BaseAction subclass.
 */

/**
 * Each Preference stores information about a preference that an
 * experiment sets.
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
 * @property {boolean} overridden
 *   Tracks if this preference has been changed away from the experimental value.
 */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "CleanupManager",
  "resource://normandy/lib/CleanupManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "JSONFile",
  "resource://gre/modules/JSONFile.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "LogManager",
  "resource://normandy/lib/LogManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TelemetryEnvironment",
  "resource://gre/modules/TelemetryEnvironment.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TelemetryEvents",
  "resource://normandy/lib/TelemetryEvents.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "NormandyUtils",
  "resource://normandy/lib/NormandyUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrefUtils",
  "resource://normandy/lib/PrefUtils.jsm"
);

var EXPORTED_SYMBOLS = ["PreferenceExperiments"];

const EXPERIMENT_FILE = "shield-preference-experiments.json";
const STARTUP_EXPERIMENT_PREFS_BRANCH = "app.normandy.startupExperimentPrefs.";

const MAX_EXPERIMENT_TYPE_LENGTH = 20; // enforced by TelemetryEnvironment
const EXPERIMENT_TYPE_PREFIX = "normandy-";
const MAX_EXPERIMENT_SUBTYPE_LENGTH =
  MAX_EXPERIMENT_TYPE_LENGTH - EXPERIMENT_TYPE_PREFIX.length;

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
let gStorePromise;
function ensureStorage() {
  if (gStorePromise === undefined) {
    const path = PathUtils.join(
      Services.dirsvc.get("ProfD", Ci.nsIFile).path,
      EXPERIMENT_FILE
    );
    const storage = new JSONFile({ path });
    // `storage.load()` is defined as being infallible: It won't ever throw an
    // error. However, if there are are I/O errors, such as a corrupt, missing,
    // or unreadable file the data loaded will be an empty object. This can
    // happen ever after our migrations have run. If that happens, edit the
    // storage to match our expected schema before returning it to the rest of
    // the module.
    gStorePromise = storage.load().then(() => {
      if (!storage.data.experiments) {
        storage.data = { ...storage.data, experiments: {} };
      }
      return storage;
    });
  }
  return gStorePromise;
}

const log = LogManager.getLogger("preference-experiments");

// List of active preference observers. Cleaned up on shutdown.
let experimentObservers = new Map();
CleanupManager.addCleanupHandler(() =>
  PreferenceExperiments.stopAllObservers()
);

var PreferenceExperiments = {
  /**
   * Update the the experiment storage with changes that happened during early startup.
   * @param {object} studyPrefsChanged Map from pref name to previous pref value
   */
  async recordOriginalValues(studyPrefsChanged) {
    const store = await ensureStorage();

    for (const experiment of Object.values(store.data.experiments)) {
      for (const [prefName, prefInfo] of Object.entries(
        experiment.preferences
      )) {
        if (studyPrefsChanged.hasOwnProperty(prefName)) {
          if (experiment.expired) {
            log.warn(
              "Expired preference experiment changed value during startup"
            );
          }
          if (prefInfo.preferenceBranch !== "default") {
            log.warn(
              "Non-default branch preference experiment changed value during startup"
            );
          }
          prefInfo.previousPreferenceValue = studyPrefsChanged[prefName];
        }
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
    CleanupManager.addCleanupHandler(() => this.saveStartupPrefs());

    for (const experiment of await this.getAllActive()) {
      // Check that the current value of the preference is still what we set it to
      for (const [preferenceName, spec] of Object.entries(
        experiment.preferences
      )) {
        if (
          !spec.overridden &&
          PrefUtils.getPref(preferenceName) !== spec.preferenceValue
        ) {
          // if not, record the difference
          await this.recordPrefChange({
            experiment,
            preferenceName,
            reason: "sideload",
          });
        }
      }

      // Notify Telemetry of experiments we're running, since they don't persist between restarts
      TelemetryEnvironment.setExperimentActive(
        experiment.slug,
        experiment.branch,
        {
          type: EXPERIMENT_TYPE_PREFIX + experiment.experimentType,
          enrollmentId:
            experiment.enrollmentId || TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
        }
      );

      // Watch for changes to the experiment's preference
      this.startObserver(experiment.slug, experiment.preferences);
    }
  },

  /**
   * Save in-progress, default-branch preference experiments in a sub-branch of
   * the normandy preferences. On startup, we read these to set the
   * experimental values.
   *
   * This is needed because the default branch does not persist between Firefox
   * restarts. To compensate for that, Normandy sets the default branch to the
   * experiment values again every startup. The values to set the preferences
   * to are stored in user-branch preferences because preferences have minimal
   * impact on the performance of startup.
   */
  async saveStartupPrefs() {
    const prefBranch = Services.prefs.getBranch(
      STARTUP_EXPERIMENT_PREFS_BRANCH
    );
    for (const pref of prefBranch.getChildList("")) {
      prefBranch.clearUserPref(pref);
    }

    // Only store prefs to set on the default branch.
    // Be careful not to store user branch prefs here, because this
    // would cause the default branch to match the user branch,
    // causing the user branch pref to get cleared.
    const allExperiments = await this.getAllActive();
    const defaultBranchPrefs = allExperiments
      .flatMap(exp => Object.entries(exp.preferences))
      .filter(
        ([preferenceName, preferenceInfo]) =>
          preferenceInfo.preferenceBranchType === "default"
      );
    for (const [preferenceName, { preferenceValue }] of defaultBranchPrefs) {
      switch (typeof preferenceValue) {
        case "string":
          prefBranch.setCharPref(preferenceName, preferenceValue);
          break;

        case "number":
          prefBranch.setIntPref(preferenceName, preferenceValue);
          break;

        case "boolean":
          prefBranch.setBoolPref(preferenceName, preferenceValue);
          break;

        default:
          throw new Error(`Invalid preference type ${typeof preferenceValue}`);
      }
    }
  },

  /**
   * Test wrapper that temporarily replaces the stored experiment data with fake
   * data for testing.
   */
  withMockExperiments(prefExperiments = []) {
    return function wrapper(testFunction) {
      return async function wrappedTestFunction(args) {
        const experiments = {};

        for (const exp of prefExperiments) {
          if (exp.name) {
            throw new Error(
              "Preference experiments 'name' field has been replaced by 'slug' and 'userFacingName', please update."
            );
          }

          experiments[exp.slug] = exp;
        }
        const data = { experiments };

        const oldPromise = gStorePromise;
        gStorePromise = Promise.resolve({
          data,
          saveSoon() {},
        });
        const oldObservers = experimentObservers;
        experimentObservers = new Map();
        try {
          await testFunction({ ...args, prefExperiments });
        } finally {
          gStorePromise = oldPromise;
          PreferenceExperiments.stopAllObservers();
          experimentObservers = oldObservers;
        }
      };
    };
  },

  /** When Telemetry is disabled, clear all identifiers from the stored experiments.  */
  async onTelemetryDisabled() {
    const store = await ensureStorage();
    for (const experiment of Object.values(store.data.experiments)) {
      experiment.enrollmentId = TelemetryEvents.NO_ENROLLMENT_ID_MARKER;
    }
    store.saveSoon();
  },

  /**
   * Clear all stored data about active and past experiments.
   */
  async clearAllExperimentStorage() {
    const store = await ensureStorage();
    store.data = {
      experiments: {},
    };
    store.saveSoon();
  },

  /**
   * Start a new preference experiment.
   * @param {Object} experiment
   * @param {string} experiment.slug
   * @param {string} experiment.actionName  The action who knows about this
   *   experiment and is responsible for cleaning it up. This should
   *   correspond to the name of some BaseAction subclass.
   * @param {string} experiment.branch
   * @param {string} experiment.preferenceName
   * @param {string|integer|boolean} experiment.preferenceValue
   * @param {PreferenceBranchType} experiment.preferenceBranchType
   * @returns {Experiment} The experiment object stored in the data store
   * @rejects {Error}
   *   - If an experiment with the given name already exists
   *   - if an experiment for the given preference is active
   *   - If the given preferenceType does not match the existing stored preference
   */
  async start({
    name = null, // To check if old code is still using `name` instead of `slug`, and provide a nice error message
    slug,
    actionName,
    branch,
    preferences,
    experimentType = "exp",
    userFacingName = null,
    userFacingDescription = null,
  }) {
    if (name) {
      throw new Error(
        "Preference experiments 'name' field has been replaced by 'slug' and 'userFacingName', please update."
      );
    }

    log.debug(`PreferenceExperiments.start(${slug}, ${branch})`);

    const store = await ensureStorage();
    if (slug in store.data.experiments) {
      TelemetryEvents.sendEvent("enrollFailed", "preference_study", slug, {
        reason: "name-conflict",
      });
      throw new Error(
        `A preference experiment with the slug "${slug}" already exists.`
      );
    }

    const activeExperiments = Object.values(store.data.experiments).filter(
      e => !e.expired
    );
    const preferencesWithConflicts = Object.keys(preferences).filter(
      preferenceName => {
        return activeExperiments.some(e =>
          e.preferences.hasOwnProperty(preferenceName)
        );
      }
    );

    if (preferencesWithConflicts.length) {
      TelemetryEvents.sendEvent("enrollFailed", "preference_study", slug, {
        reason: "pref-conflict",
      });
      throw new Error(
        `Another preference experiment for the pref "${preferencesWithConflicts[0]}" is currently active.`
      );
    }

    if (experimentType.length > MAX_EXPERIMENT_SUBTYPE_LENGTH) {
      TelemetryEvents.sendEvent("enrollFailed", "preference_study", slug, {
        reason: "experiment-type-too-long",
      });
      throw new Error(
        `experimentType must be less than ${MAX_EXPERIMENT_SUBTYPE_LENGTH} characters. ` +
          `"${experimentType}" is ${experimentType.length} long.`
      );
    }

    // Sanity check each preference
    for (const [preferenceName, preferenceInfo] of Object.entries(
      preferences
    )) {
      // Ensure preferenceBranchType is set, using the default from
      // the schema. This also modifies the preferenceInfo for use in
      // the rest of the function.
      preferenceInfo.preferenceBranchType =
        preferenceInfo.preferenceBranchType || "default";
      const { preferenceBranchType, preferenceType } = preferenceInfo;
      if (
        !(preferenceBranchType === "user" || preferenceBranchType === "default")
      ) {
        TelemetryEvents.sendEvent("enrollFailed", "preference_study", slug, {
          reason: "invalid-branch",
          prefBranch: preferenceBranchType.slice(0, 80),
        });
        throw new Error(
          `Invalid value for preferenceBranchType: ${preferenceBranchType}`
        );
      }

      const prevPrefType = Services.prefs.getPrefType(preferenceName);
      const givenPrefType = PREFERENCE_TYPE_MAP[preferenceType];

      if (!preferenceType || !givenPrefType) {
        TelemetryEvents.sendEvent("enrollFailed", "preference_study", slug, {
          reason: "invalid-type",
        });
        throw new Error(
          `Invalid preferenceType provided (given "${preferenceType}")`
        );
      }

      if (
        prevPrefType !== Services.prefs.PREF_INVALID &&
        prevPrefType !== givenPrefType
      ) {
        TelemetryEvents.sendEvent("enrollFailed", "preference_study", slug, {
          reason: "invalid-type",
        });
        throw new Error(
          `Previous preference value is of type "${prevPrefType}", but was given ` +
            `"${givenPrefType}" (${preferenceType})`
        );
      }

      preferenceInfo.previousPreferenceValue = PrefUtils.getPref(
        preferenceName,
        { branch: preferenceBranchType }
      );
    }

    const alreadyOverriddenPrefs = new Set();
    for (const [preferenceName, preferenceInfo] of Object.entries(
      preferences
    )) {
      const { preferenceValue, preferenceBranchType } = preferenceInfo;
      if (
        preferenceBranchType === "default" &&
        Services.prefs.prefHasUserValue(preferenceName)
      ) {
        alreadyOverriddenPrefs.add(preferenceName);
      }
      PrefUtils.setPref(preferenceName, preferenceValue, {
        branch: preferenceBranchType,
      });
    }
    PreferenceExperiments.startObserver(slug, preferences);

    const enrollmentId = NormandyUtils.generateUuid();

    /** @type {Experiment} */
    const experiment = {
      slug,
      actionName,
      branch,
      expired: false,
      lastSeen: new Date().toJSON(),
      preferences,
      experimentType,
      userFacingName,
      userFacingDescription,
      enrollmentId,
    };

    store.data.experiments[slug] = experiment;
    store.saveSoon();

    // Record telemetry that the experiment started
    TelemetryEnvironment.setExperimentActive(slug, branch, {
      type: EXPERIMENT_TYPE_PREFIX + experimentType,
      enrollmentId: enrollmentId || TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
    });
    TelemetryEvents.sendEvent("enroll", "preference_study", slug, {
      experimentType,
      branch,
      enrollmentId: enrollmentId || TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
    });

    // Send events for any default branch preferences set that already had user
    // values overriding them.
    for (const preferenceName of alreadyOverriddenPrefs) {
      await this.recordPrefChange({
        experiment,
        preferenceName,
        reason: "onEnroll",
      });
    }
    await this.saveStartupPrefs();

    return experiment;
  },

  /**
   * Register a preference observer that stops an experiment when the user
   * modifies the preference.
   * @param {string} experimentSlug
   * @param {string} preferenceName
   * @param {string|integer|boolean} preferenceValue
   * @throws {Error}
   *   If an observer for the experiment is already active.
   */
  startObserver(experimentSlug, preferences) {
    log.debug(`PreferenceExperiments.startObserver(${experimentSlug})`);

    if (experimentObservers.has(experimentSlug)) {
      throw new Error(
        `An observer for the preference experiment ${experimentSlug} is already active.`
      );
    }

    const observerInfo = {
      preferences,
      observe(aSubject, aTopic, preferenceName) {
        const prefInfo = preferences[preferenceName];
        // if `preferenceName` is one of the experiment prefs but with more on
        // the end (ie, foo.bar vs foo.bar.baz) then this can be triggered for
        // changes we don't care about. Check for that.
        if (!prefInfo) {
          return;
        }
        const originalValue = prefInfo.preferenceValue;
        const newValue = PrefUtils.getPref(preferenceName);
        if (newValue !== originalValue) {
          PreferenceExperiments.recordPrefChange({
            experimentSlug,
            preferenceName,
            reason: "observer",
          });
          Services.prefs.removeObserver(preferenceName, observerInfo);
        }
      },
    };
    experimentObservers.set(experimentSlug, observerInfo);
    for (const [preferenceName, spec] of Object.entries(preferences)) {
      if (!spec.overridden) {
        Services.prefs.addObserver(preferenceName, observerInfo);
      }
    }
  },

  /**
   * Check if a preference observer is active for an experiment.
   * @param {string} experimentSlug
   * @return {Boolean}
   */
  hasObserver(experimentSlug) {
    log.debug(`PreferenceExperiments.hasObserver(${experimentSlug})`);
    return experimentObservers.has(experimentSlug);
  },

  /**
   * Disable a preference observer for an experiment.
   * @param {string} experimentSlug
   * @throws {Error}
   *   If there is no active observer for the experiment.
   */
  stopObserver(experimentSlug) {
    log.debug(`PreferenceExperiments.stopObserver(${experimentSlug})`);

    if (!experimentObservers.has(experimentSlug)) {
      throw new Error(
        `No observer for the preference experiment ${experimentSlug} found.`
      );
    }

    const observer = experimentObservers.get(experimentSlug);
    for (const preferenceName of Object.keys(observer.preferences)) {
      Services.prefs.removeObserver(preferenceName, observer);
    }
    experimentObservers.delete(experimentSlug);
  },

  /**
   * Disable all currently-active preference observers for experiments.
   */
  stopAllObservers() {
    log.debug("PreferenceExperiments.stopAllObservers()");
    for (const observer of experimentObservers.values()) {
      for (const preferenceName of Object.keys(observer.preferences)) {
        Services.prefs.removeObserver(preferenceName, observer);
      }
    }
    experimentObservers.clear();
  },

  /**
   * Update the timestamp storing when Normandy last sent a recipe for the
   * experiment.
   * @param {string} experimentSlug
   * @rejects {Error}
   *   If there is no stored experiment with the given slug.
   */
  async markLastSeen(experimentSlug) {
    log.debug(`PreferenceExperiments.markLastSeen(${experimentSlug})`);

    const store = await ensureStorage();
    if (!(experimentSlug in store.data.experiments)) {
      throw new Error(
        `Could not find a preference experiment with the slug "${experimentSlug}"`
      );
    }

    store.data.experiments[experimentSlug].lastSeen = new Date().toJSON();
    store.saveSoon();
  },

  /**
   * Called when an experimental pref has changed away from its experimental
   * value for the first time.
   *
   * One of `experiment` or `slug` must be passed.
   *
   * @param {object} options
   * @param {Experiment} [options.experiment]
   *   The experiment that had a pref change. If this is passed, slug is ignored.
   * @param {string} [options.slug]
   *   The slug of the experiment that had a pref change. This will be used to
   *   fetch an experiment if none was passed.
   * @param {string} options.preferenceName The preference changed.
   * @param {string} options.reason The reason the preference change was detected.
   */
  async recordPrefChange({
    experiment = null,
    experimentSlug = null,
    preferenceName,
    reason,
  }) {
    if (!experiment) {
      experiment = await PreferenceExperiments.get(experimentSlug);
    }
    let preferenceSpecification = experiment.preferences[preferenceName];
    if (!preferenceSpecification) {
      throw new PreferenceExperiments.InvalidPreferenceName(
        `Preference "${preferenceName}" is not a part of experiment "${experimentSlug}"`
      );
    }

    preferenceSpecification.overridden = true;
    await this.update(experiment);

    TelemetryEvents.sendEvent(
      "expPrefChanged",
      "preference_study",
      experiment.slug,
      {
        preferenceName,
        reason,
        enrollmentId:
          experiment.enrollmentId || TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
      }
    );
  },

  /**
   * Stop an active experiment, deactivate preference watchers, and optionally
   * reset the associated preference to its previous value.
   * @param {string} experimentSlug
   * @param {Object} options
   * @param {boolean} [options.resetValue = true]
   *   If true, reset the preference to its original value prior to
   *   the experiment. Optional, defaults to true.
   * @param {String} [options.reason = "unknown"]
   *   Reason that the experiment is ending. Optional, defaults to
   *   "unknown".
   * @rejects {Error}
   *   If there is no stored experiment with the given slug, or if the
   *   experiment has already expired.
   */
  async stop(
    experimentSlug,
    { resetValue = true, reason = "unknown", changedPref, caller } = {}
  ) {
    log.debug(
      `PreferenceExperiments.stop(${experimentSlug}, {resetValue: ${resetValue}, reason: ${reason}, changedPref: ${changedPref}, caller: ${caller}})`
    );
    if (reason === "unknown") {
      log.warn(`experiment ${experimentSlug} ending for unknown reason`);
    }

    const store = await ensureStorage();
    if (!(experimentSlug in store.data.experiments)) {
      TelemetryEvents.sendEvent(
        "unenrollFailed",
        "preference_study",
        experimentSlug,
        {
          reason: "does-not-exist",
          originalReason: reason,
          ...(changedPref ? { changedPref } : {}),
        }
      );
      throw new Error(
        `Could not find a preference experiment with the slug "${experimentSlug}"`
      );
    }

    const experiment = store.data.experiments[experimentSlug];
    if (experiment.expired) {
      const extra = {
        reason: "already-unenrolled",
        originalReason: reason,
        enrollmentId:
          experiment.enrollmentId || TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
      };
      if (changedPref) {
        extra.changedPref = changedPref;
      }
      if (caller && AppConstants.NIGHTLY_BUILD) {
        extra.caller = caller;
      }
      TelemetryEvents.sendEvent(
        "unenrollFailed",
        "preference_study",
        experimentSlug,
        extra
      );
      throw new Error(
        `Cannot stop preference experiment "${experimentSlug}" because it is already expired`
      );
    }

    if (PreferenceExperiments.hasObserver(experimentSlug)) {
      PreferenceExperiments.stopObserver(experimentSlug);
    }

    if (resetValue) {
      for (const [preferenceName, prefInfo] of Object.entries(
        experiment.preferences
      )) {
        const { previousPreferenceValue, preferenceBranchType } = prefInfo;
        const preferences = PreferenceBranchType[preferenceBranchType];

        if (previousPreferenceValue !== null) {
          PrefUtils.setPref(preferenceName, previousPreferenceValue, {
            branch: preferenceBranchType,
          });
        } else if (preferenceBranchType === "user") {
          // Remove the "user set" value (which Shield set), but leave the default intact.
          preferences.clearUserPref(preferenceName);
        } else {
          log.warn(
            `Can't revert pref ${preferenceName} for experiment ${experimentSlug} ` +
              `because it had no default value. ` +
              `Preference will be reset at the next restart.`
          );
          // It would seem that Services.prefs.deleteBranch() could be used for
          // this, but in Normandy's case it does not work. See bug 1502410.
        }
      }
    }

    experiment.expired = true;
    if (experiment.temporaryErrorDeadline) {
      experiment.temporaryErrorDeadline = null;
    }
    await store.saveSoon();

    TelemetryEnvironment.setExperimentInactive(experimentSlug);
    TelemetryEvents.sendEvent("unenroll", "preference_study", experimentSlug, {
      didResetValue: resetValue ? "true" : "false",
      branch: experiment.branch,
      reason,
      enrollmentId:
        experiment.enrollmentId || TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
      ...(changedPref ? { changedPref } : {}),
    });
    await this.saveStartupPrefs();
    Services.obs.notifyObservers(
      null,
      "normandy:preference-experiment:stopped",
      experimentSlug
    );
  },

  /**
   * Clone an experiment using knowledge of its structure to avoid
   * having to serialize/deserialize it.
   *
   * We do this in places where return experiments so clients can't
   * accidentally mutate our data underneath us.
   */
  _cloneExperiment(experiment) {
    return {
      ...experiment,
      preferences: {
        ...experiment.preferences,
      },
    };
  },

  /**
   * Get the experiment object for the experiment.
   * @param {string} experimentSlug
   * @resolves {Experiment}
   * @rejects {Error}
   *   If no preference experiment exists with the given slug.
   */
  async get(experimentSlug) {
    log.debug(`PreferenceExperiments.get(${experimentSlug})`);
    const store = await ensureStorage();
    if (!(experimentSlug in store.data.experiments)) {
      throw new PreferenceExperiments.NotFoundError(
        `Could not find a preference experiment with the slug "${experimentSlug}"`
      );
    }

    return this._cloneExperiment(store.data.experiments[experimentSlug]);
  },

  /**
   * Get a list of all stored experiment objects.
   * @resolves {Experiment[]}
   */
  async getAll() {
    const store = await ensureStorage();
    return Object.values(store.data.experiments).map(experiment =>
      this._cloneExperiment(experiment)
    );
  },

  /**
   * Get a list of experiment objects for all active experiments.
   * @resolves {Experiment[]}
   */
  async getAllActive() {
    const store = await ensureStorage();
    return Object.values(store.data.experiments)
      .filter(e => !e.expired)
      .map(e => this._cloneExperiment(e));
  },

  /**
   * Check if an experiment exists with the given slug.
   * @param {string} experimentSlug
   * @resolves {boolean} True if the experiment exists, false if it doesn't.
   */
  async has(experimentSlug) {
    log.debug(`PreferenceExperiments.has(${experimentSlug})`);
    const store = await ensureStorage();
    return experimentSlug in store.data.experiments;
  },

  /**
   * Update an experiment in the data store. If an experiment with the given
   * slug is not already in the store, an error will be thrown.
   *
   * @param experiment {Experiment} The experiment to update
   * @param experiment.slug {String} The experiment must have a slug
   */
  async update(experiment) {
    const store = await ensureStorage();

    if (!(experiment.slug in store.data.experiments)) {
      throw new Error(
        `Could not update a preference experiment with the slug "${experiment.slug}"`
      );
    }

    store.data.experiments[experiment.slug] = experiment;
    store.saveSoon();
  },

  NotFoundError: class extends Error {},
  InvalidPreferenceName: class extends Error {},

  /**
   * These migrations should only be called from `NormandyMigrations.jsm` and tests.
   */
  migrations: {
    /** Move experiments into a specific key. */
    async migration01MoveExperiments(storage = null) {
      if (storage === null) {
        storage = await ensureStorage();
      }
      if (Object.hasOwnProperty.call(storage.data, "experiments")) {
        return;
      }
      storage.data = {
        experiments: storage.data,
      };
      delete storage.data.experiments.__version;
      storage.saveSoon();
    },

    /** Migrate storage.data to multi-preference format */
    async migration02MultiPreference(storage = null) {
      if (storage === null) {
        storage = await ensureStorage();
      }

      const oldExperiments = storage.data.experiments;
      const v2Experiments = {};

      for (let [expName, oldExperiment] of Object.entries(oldExperiments)) {
        if (expName == "__version") {
          // A stray "__version" entry snuck in, likely from old migrations.
          // Ignore it and continue. It won't be propagated to future
          // migrations, since `v2Experiments` won't have it.
          continue;
        }
        if (oldExperiment.preferences) {
          // experiment is already migrated
          v2Experiments[expName] = oldExperiment;
          continue;
        }
        v2Experiments[expName] = {
          name: oldExperiment.name,
          branch: oldExperiment.branch,
          expired: oldExperiment.expired,
          lastSeen: oldExperiment.lastSeen,
          preferences: {
            [oldExperiment.preferenceName]: {
              preferenceBranchType: oldExperiment.preferenceBranchType,
              preferenceType: oldExperiment.preferenceType,
              preferenceValue: oldExperiment.preferenceValue,
              previousPreferenceValue: oldExperiment.previousPreferenceValue,
            },
          },
          experimentType: oldExperiment.experimentType,
        };
      }
      storage.data.experiments = v2Experiments;
      storage.saveSoon();
    },

    /** Add "actionName" field for experiments that don't have it. */
    async migration03AddActionName(storage = null) {
      if (storage === null) {
        storage = await ensureStorage();
      }

      for (const experiment of Object.values(storage.data.experiments)) {
        if (!experiment.actionName) {
          // Assume SinglePreferenceExperimentAction because as of this
          // writing, no multi-pref experiment recipe has launched.
          experiment.actionName = "SinglePreferenceExperimentAction";
        }
      }
      storage.saveSoon();
    },

    async migration04RenameNameToSlug(storage = null) {
      if (!storage) {
        storage = await ensureStorage();
      }
      // Rename "name" to "slug" to match the intended purpose of the field.
      for (const experiment of Object.values(storage.data.experiments)) {
        if (experiment.name && !experiment.slug) {
          experiment.slug = experiment.name;
          delete experiment.name;
        }
      }
      storage.saveSoon();
    },

    async migration05RemoveOldAction() {
      const experiments = await PreferenceExperiments.getAllActive();
      for (const experiment of experiments) {
        if (experiment.actionName == "SinglePreferenceExperimentAction") {
          try {
            await PreferenceExperiments.stop(experiment.slug, {
              resetValue: true,
              reason: "migration-removing-single-pref-action",
              caller: "migration05RemoveOldAction",
            });
          } catch (e) {
            log.error(
              `Stopping preference experiment ${experiment.slug} during migration failed: ${e}`
            );
          }
        }
      }
    },

    async migration06TrackOverriddenPrefs(storage = null) {
      if (!storage) {
        storage = await ensureStorage();
      }
      for (const experiment of Object.values(storage.data.experiments)) {
        for (const [preferenceName, specification] of Object.entries(
          experiment.preferences
        )) {
          if (specification.overridden !== undefined) {
            continue;
          }
          specification.overridden =
            PrefUtils.getPref(preferenceName) !== specification.preferenceValue;
        }
      }
      storage.saveSoon();
    },
  },
};
