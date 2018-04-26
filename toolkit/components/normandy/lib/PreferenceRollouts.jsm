/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://normandy/actions/BaseAction.jsm");
ChromeUtils.import("resource://normandy/lib/LogManager.jsm");
ChromeUtils.defineModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "IndexedDB", "resource://gre/modules/IndexedDB.jsm");
ChromeUtils.defineModuleGetter(this, "TelemetryEnvironment", "resource://gre/modules/TelemetryEnvironment.jsm");
ChromeUtils.defineModuleGetter(this, "PrefUtils", "resource://normandy/lib/PrefUtils.jsm");
ChromeUtils.defineModuleGetter(this, "TelemetryEvents", "resource://normandy/lib/TelemetryEvents.jsm");

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
const DB_OPTIONS = {version: 1};

/**
 * Create a new connection to the database.
 */
function openDatabase() {
  return IndexedDB.open(DB_NAME, DB_OPTIONS, db => {
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
 * NOTE: Methods on the store returned by this function MUST be called
 * synchronously, otherwise the transaction with the store will expire.
 * This is why the helper takes a database as an argument; if we fetched the
 * database in the helper directly, the helper would be async and the
 * transaction would expire before methods on the store were called.
 */
function getStore(db) {
  return db.objectStore(STORE_NAME, "readwrite");
}

var PreferenceRollouts = {
  STATE_ACTIVE: "active",
  STATE_ROLLED_BACK: "rolled-back",
  STATE_GRADUATED: "graduated",

  /**
   * Update the rollout database with changes that happened during early startup.
   * @param {object} rolloutPrefsChanged Map from pref name to previous pref value
   */
  async recordOriginalValues(originalPreferences) {
    for (const rollout of await this.getAllActive()) {
      let changed = false;

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
          changed = true;
        }
      }

      if (prefMatchingDefaultCount === rollout.preferences.length) {
        // Firefox's builtin defaults have caught up to the rollout, making all
        // of the rollout's changes redundant, so graduate the rollout.
        rollout.state = this.STATE_GRADUATED;
        changed = true;
        log.debug(`Graduating rollout: ${rollout.slug}`);
        TelemetryEvents.sendEvent("graduate", "preference_rollout", rollout.slug, {});
      }

      if (changed) {
        const db = await getDatabase();
        await getStore(db).put(rollout);
      }
    }
  },

  async init() {
    for (const rollout of await this.getAllActive()) {
      TelemetryEnvironment.setExperimentActive(rollout.slug, rollout.state, {type: "normandy-prefrollout"});
    }
  },

  async uninit() {
    await this.saveStartupPrefs();
  },

  /**
   * Test wrapper that temporarily replaces the stored rollout data with fake
   * data for testing.
   */
  withTestMock(testFunction) {
    return async function inner(...args) {
      let db = await getDatabase();
      const oldData = await getStore(db).getAll();
      await getStore(db).clear();
      try {
        await testFunction(...args);
      } finally {
        db = await getDatabase();
        const store = getStore(db);
        let promises = [store.clear()];
        for (const d of oldData) {
          promises.push(store.add(d));
        }
        await Promise.all(promises);
      }
    };
  },

  /**
   * Add a new rollout
   * @param {PreferenceRollout} rollout
   */
  async add(rollout) {
    const db = await getDatabase();
    return getStore(db).add(rollout);
  },

  /**
   * Update an existing rollout
   * @param {PreferenceRollout} rollout
   * @throws If a matching rollout does not exist.
   */
  async update(rollout) {
    if (!await this.has(rollout.slug)) {
      throw new Error(`Tried to update ${rollout.slug}, but it doesn't already exist.`);
    }
    const db = await getDatabase();
    return getStore(db).put(rollout);
  },

  /**
   * Test whether there is a rollout in storage with the given slug.
   * @param {string} slug
   * @returns {boolean}
   */
  async has(slug) {
    const db = await getDatabase();
    const rollout = await getStore(db).get(slug);
    return !!rollout;
  },

  /**
   * Get a rollout by slug
   * @param {string} slug
   */
  async get(slug) {
    const db = await getDatabase();
    return getStore(db).get(slug);
  },

  /** Get all rollouts in the database. */
  async getAll() {
    const db = await getDatabase();
    return getStore(db).getAll();
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
    prefBranch.deleteBranch("");

    for (const rollout of await this.getAllActive()) {
      for (const prefSpec of rollout.preferences) {
        PrefUtils.setPref("user", STARTUP_PREFS_BRANCH + prefSpec.preferenceName, prefSpec.value);
      }
    }
  },

  /**
   * Close the current database connection if it is open. If it is not open,
   * this is a no-op.
   */
  async closeDB() {
    if (databasePromise) {
      const promise = databasePromise;
      databasePromise = null;
      const db = await promise;
      await db.close();
    }
  },
};
