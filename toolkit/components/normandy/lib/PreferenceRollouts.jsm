/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { LogManager } = ChromeUtils.import(
  "resource://normandy/lib/LogManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "IndexedDB",
  "resource://gre/modules/IndexedDB.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TelemetryEnvironment",
  "resource://gre/modules/TelemetryEnvironment.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "CleanupManager",
  "resource://normandy/lib/CleanupManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrefUtils",
  "resource://normandy/lib/PrefUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "TelemetryEvents",
  "resource://normandy/lib/TelemetryEvents.jsm"
);

const log = LogManager.getLogger("recipe-runner");

/**
 * PreferenceRollouts store info about an active or expired preference rollout.
 * @typedef {object} PreferenceRollout
 * @property {string} slug
 *   Unique slug of the experiment
 * @property {string} state
 *   The current state of the rollout: "active", "rolled-back", "graduated".
 *   Active means that Normandy is actively managing therollout. Rolled-back
 *   means that the rollout was previously active, but has been rolled back for
 *   this user. Graduated means that the built-in default now matches the
 *   rollout value, and so Normandy is no longer managing the preference.
 * @property {Array<PreferenceSpec>} preferences
 *   An array of preferences specifications involved in the rollout.
 * @property {string} enrollmentId
 *   A random ID generated at time of enrollment. It should be included on all
 *   telemetry related to this rollout. It should not be re-used by other
 *   rollouts, or any other purpose. May be null on old rollouts.
 */

/**
 * PreferenceSpec describe how a preference should change during a rollout.
 * @typedef {object} PreferenceSpec
 * @property {string} preferenceName
 *   The preference to modify.
 * @property {string} preferenceType
 *   Type of the preference being set.
 * @property {string|integer|boolean} value
 *   The value to change the preference to.
 * @property {string|integer|boolean} previousValue
 *   The value the preference would have on the default branch if this rollout
 *   were not active.
 */

var EXPORTED_SYMBOLS = ["PreferenceRollouts"];
const STARTUP_PREFS_BRANCH = "app.normandy.startupRolloutPrefs.";
const DB_NAME = "normandy-preference-rollout";
const STORE_NAME = "preference-rollouts";
const DB_VERSION = 1;

/**
 * Create a new connection to the database.
 */
function openDatabase() {
  return IndexedDB.open(DB_NAME, DB_VERSION, db => {
    db.createObjectStore(STORE_NAME, {
      keyPath: "slug",
    });
  });
}

/**
 * Cache the database connection so that it is shared among multiple operations.
 */
let databasePromise;
function getDatabase() {
  if (!databasePromise) {
    databasePromise = openDatabase();
  }
  return databasePromise;
}

/**
 * Get a transaction for interacting with the rollout store.
 *
 * @param {IDBDatabase} db
 * @param {String} mode Either "readonly" or "readwrite"
 *
 * NOTE: Methods on the store returned by this function MUST be called
 * synchronously, otherwise the transaction with the store will expire.
 * This is why the helper takes a database as an argument; if we fetched the
 * database in the helper directly, the helper would be async and the
 * transaction would expire before methods on the store were called.
 */
function getStore(db, mode) {
  if (!mode) {
    throw new Error("mode is required");
  }
  return db.objectStore(STORE_NAME, mode);
}

var PreferenceRollouts = {
  STATE_ACTIVE: "active",
  STATE_ROLLED_BACK: "rolled-back",
  STATE_GRADUATED: "graduated",

  // A set of rollout slugs that are obsolete based the code in this build of
  // Firefox. This may include things like the preference no longer being
  // applicable, or the feature changing in such a way that Normandy's automatic
  // graduation system cannot detect that the rollout should hand off to the
  // built-in code.
  GRADUATION_SET: new Set(),

  /**
   * Update the rollout database with changes that happened during early startup.
   * @param {object} rolloutPrefsChanged Map from pref name to previous pref value
   */
  async recordOriginalValues(originalPreferences) {
    for (const rollout of await this.getAllActive()) {
      let shouldSaveRollout = false;

      // Count the number of preferences in this rollout that are now redundant.
      let prefMatchingDefaultCount = 0;

      for (const prefSpec of rollout.preferences) {
        const builtInDefault = originalPreferences[prefSpec.preferenceName];
        if (prefSpec.value === builtInDefault) {
          prefMatchingDefaultCount++;
        }
        // Store the current built-in default. That way, if the preference is
        // rolled back during the current session (ie, until the browser is
        // shut down), the correct value will be used.
        if (prefSpec.previousValue !== builtInDefault) {
          prefSpec.previousValue = builtInDefault;
          shouldSaveRollout = true;
        }
      }

      if (prefMatchingDefaultCount === rollout.preferences.length) {
        // Firefox's builtin defaults have caught up to the rollout, making all
        // of the rollout's changes redundant, so graduate the rollout.
        await this.graduate(rollout, "all-prefs-match");
        // `this.graduate` writes the rollout to the db, so we don't need to do it anymore.
        shouldSaveRollout = false;
      }

      if (shouldSaveRollout) {
        const db = await getDatabase();
        await getStore(db, "readwrite").put(rollout);
      }
    }
  },

  async init() {
    CleanupManager.addCleanupHandler(() => this.saveStartupPrefs());

    for (const rollout of await this.getAllActive()) {
      if (this.GRADUATION_SET.has(rollout.slug)) {
        await this.graduate(rollout, "in-graduation-set");
        continue;
      }
      TelemetryEnvironment.setExperimentActive(rollout.slug, rollout.state, {
        type: "normandy-prefrollout",
        enrollmentId:
          rollout.enrollmentId || TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
      });
    }
  },

  /** When Telemetry is disabled, clear all identifiers from the stored rollouts.  */
  async onTelemetryDisabled() {
    const rollouts = await this.getAll();
    for (const rollout of rollouts) {
      rollout.enrollmentId = TelemetryEvents.NO_ENROLLMENT_ID_MARKER;
    }
    await this.updateMany(rollouts);
  },

  /**
   * Test wrapper that temporarily replaces the stored rollout data with fake
   * data for testing.
   */
  withTestMock({
    graduationSet = new Set(),
    rollouts: prefRollouts = [],
  } = {}) {
    return testFunction => {
      return async args => {
        let db = await getDatabase();
        const oldData = await getStore(db, "readonly").getAll();
        await getStore(db, "readwrite").clear();
        await Promise.all(prefRollouts.map(r => this.add(r)));
        const oldGraduationSet = this.GRADUATION_SET;
        this.GRADUATION_SET = graduationSet;

        try {
          await testFunction({ ...args, prefRollouts });
        } finally {
          this.GRADUATION_SET = oldGraduationSet;
          db = await getDatabase();
          await getStore(db, "readwrite").clear();
          const store = getStore(db, "readwrite");
          await Promise.all(oldData.map(d => store.add(d)));
        }
      };
    };
  },

  /**
   * Add a new rollout
   * @param {PreferenceRollout} rollout
   */
  async add(rollout) {
    if (!rollout.enrollmentId) {
      throw new Error("Rollout must have an enrollment ID");
    }
    const db = await getDatabase();
    return getStore(db, "readwrite").add(rollout);
  },

  /**
   * Update an existing rollout
   * @param {PreferenceRollout} rollout
   * @throws If a matching rollout does not exist.
   */
  async update(rollout) {
    if (!(await this.has(rollout.slug))) {
      throw new Error(
        `Tried to update ${rollout.slug}, but it doesn't already exist.`
      );
    }
    const db = await getDatabase();
    return getStore(db, "readwrite").put(rollout);
  },

  /**
   * Update many existing rollouts. More efficient than calling `update` many
   * times in a row.
   * @param {Array<PreferenceRollout>} rollouts
   * @throws If any of the passed rollouts have a slug that doesn't exist in the database already.
   */
  async updateMany(rollouts) {
    // Don't touch the database if there is nothing to do
    if (!rollouts.length) {
      return;
    }

    // Both of the below operations use .map() instead of a normal loop becaues
    // once we get the object store, we can't let it expire by spinning the
    // event loop. This approach queues up all the interactions with the store
    // immediately, preventing it from expiring too soon.

    const db = await getDatabase();
    let store = await getStore(db, "readonly");
    await Promise.all(
      rollouts.map(async ({ slug }) => {
        let existingRollout = await store.get(slug);
        if (!existingRollout) {
          throw new Error(`Tried to update ${slug}, but it doesn't exist.`);
        }
      })
    );

    // awaiting spun the event loop, so the store is now invalid. Get a new
    // store. This is also a chance to get it in readwrite mode.
    store = await getStore(db, "readwrite");
    await Promise.all(rollouts.map(rollout => store.put(rollout)));
  },

  /**
   * Test whether there is a rollout in storage with the given slug.
   * @param {string} slug
   * @returns {boolean}
   */
  async has(slug) {
    const db = await getDatabase();
    const rollout = await getStore(db, "readonly").get(slug);
    return !!rollout;
  },

  /**
   * Get a rollout by slug
   * @param {string} slug
   */
  async get(slug) {
    const db = await getDatabase();
    return getStore(db, "readonly").get(slug);
  },

  /** Get all rollouts in the database. */
  async getAll() {
    const db = await getDatabase();
    return getStore(db, "readonly").getAll();
  },

  /** Get all rollouts in the "active" state. */
  async getAllActive() {
    const rollouts = await this.getAll();
    return rollouts.filter(rollout => rollout.state === this.STATE_ACTIVE);
  },

  /**
   * Save in-progress preference rollouts in a sub-branch of the normandy prefs.
   * On startup, we read these to set the rollout values.
   */
  async saveStartupPrefs() {
    const prefBranch = Services.prefs.getBranch(STARTUP_PREFS_BRANCH);
    for (const pref of prefBranch.getChildList("")) {
      prefBranch.clearUserPref(pref);
    }

    for (const rollout of await this.getAllActive()) {
      for (const prefSpec of rollout.preferences) {
        PrefUtils.setPref(
          "user",
          STARTUP_PREFS_BRANCH + prefSpec.preferenceName,
          prefSpec.value
        );
      }
    }
  },

  async graduate(rollout, reason) {
    log.debug(`Graduating rollout: ${rollout.slug}`);
    rollout.state = this.STATE_GRADUATED;
    const db = await getDatabase();
    await getStore(db, "readwrite").put(rollout);
    TelemetryEvents.sendEvent("graduate", "preference_rollout", rollout.slug, {
      reason,
      enrollmentId:
        rollout.enrollmentId || TelemetryEvents.NO_ENROLLMENT_ID_MARKER,
    });
  },
};
