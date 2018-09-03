/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * @typedef {Object} Study
 * @property {Number} recipeId
 *   ID of the recipe that created the study. Used as the primary key of the
 *   study.
 * @property {string} name
 *   Name of the study
 * @property {string} description
 *   Description of the study and its intent.
 * @property {boolean} active
 *   Is the study still running?
 * @property {string} addonId
 *   Add-on ID for this particular study.
 * @property {string} addonUrl
 *   URL that the study add-on was installed from.
 * @property {string} addonVersion
 *   Study add-on version number
 * @property {string} studyStartDate
 *   Date when the study was started.
 * @property {Date} studyEndDate
 *   Date when the study was ended.
 */

ChromeUtils.import("resource://gre/modules/osfile.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "IndexedDB", "resource://gre/modules/IndexedDB.jsm");
ChromeUtils.defineModuleGetter(this, "AddonManager", "resource://gre/modules/AddonManager.jsm");
ChromeUtils.defineModuleGetter(
  this, "CleanupManager", "resource://normandy/lib/CleanupManager.jsm"
);
ChromeUtils.defineModuleGetter(this, "LogManager", "resource://normandy/lib/LogManager.jsm");
ChromeUtils.defineModuleGetter(this, "TelemetryEvents", "resource://normandy/lib/TelemetryEvents.jsm");

var EXPORTED_SYMBOLS = ["AddonStudies"];

const DB_NAME = "shield";
const STORE_NAME = "addon-studies";
const DB_OPTIONS = {
  version: 1,
};
const STUDY_ENDED_TOPIC = "shield-study-ended";
const log = LogManager.getLogger("addon-studies");

/**
 * Create a new connection to the database.
 */
function openDatabase() {
  return IndexedDB.open(DB_NAME, DB_OPTIONS, db => {
    db.createObjectStore(STORE_NAME, {
      keyPath: "recipeId",
    });
  });
}

/**
 * Cache the database connection so that it is shared among multiple operations.
 */
let databasePromise;
async function getDatabase() {
  if (!databasePromise) {
    databasePromise = openDatabase();
  }
  return databasePromise;
}

/**
 * Get a transaction for interacting with the study store.
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

var AddonStudies = {
  /**
   * Test wrapper that temporarily replaces the stored studies with the given
   * ones. The original stored studies are restored upon completion.
   *
   * This is defined here instead of in test code since it needs to access the
   * getDatabase, which we don't expose to avoid outside modules relying on the
   * type of storage used for studies.
   *
   * @param {Array} [studies=[]]
   */
  withStudies(studies = []) {
    return function wrapper(testFunction) {
      return async function wrappedTestFunction(...args) {
        const oldStudies = await AddonStudies.getAll();
        let db = await getDatabase();
        await AddonStudies.clear();
        for (const study of studies) {
          await getStore(db).add(study);
        }
        await AddonStudies.close();

        try {
          await testFunction(...args, studies);
        } finally {
          db = await getDatabase();
          await AddonStudies.clear();
          for (const study of oldStudies) {
            await getStore(db).add(study);
          }

          await AddonStudies.close();
        }
      };
    };
  },

  async init() {
    // If an active study's add-on has been removed since we last ran, stop the
    // study.
    const activeStudies = (await this.getAll()).filter(study => study.active);
    for (const study of activeStudies) {
      const addon = await AddonManager.getAddonByID(study.addonId);
      if (!addon) {
        await this.markAsEnded(study, "uninstalled-sideload");
      }
    }
    await this.close();

    // Listen for add-on uninstalls so we can stop the corresponding studies.
    AddonManager.addAddonListener(this);
    CleanupManager.addCleanupHandler(() => {
      AddonManager.removeAddonListener(this);
    });
  },

  /**
   * If a study add-on is uninstalled, mark the study as having ended.
   * @param {Addon} addon
   */
  async onUninstalled(addon) {
    const activeStudies = (await this.getAll()).filter(study => study.active);
    const matchingStudy = activeStudies.find(study => study.addonId === addon.id);
    if (matchingStudy) {
      // Use a dedicated DB connection instead of the shared one so that we can
      // close it without fear of affecting other users of the shared connection.
      const db = await openDatabase();
      await this.markAsEnded(matchingStudy, "uninstalled");
      await db.close();
    }
  },

  /**
   * Remove all stored studies.
   */
  async clear() {
    const db = await getDatabase();
    await getStore(db).clear();
  },

  /**
   * Close the current database connection if it is open.
   */
  async close() {
    if (databasePromise) {
      const promise = databasePromise;
      databasePromise = null;
      const db = await promise;
      await db.close();
    }
  },

  /**
   * Test whether there is a study in storage for the given recipe ID.
   * @param {Number} recipeId
   * @returns {Boolean}
   */
  async has(recipeId) {
    const db = await getDatabase();
    const study = await getStore(db).get(recipeId);
    return !!study;
  },

  /**
   * Fetch a study from storage.
   * @param {Number} recipeId
   * @return {Study}
   */
  async get(recipeId) {
    const db = await getDatabase();
    return getStore(db).get(recipeId);
  },

  /**
   * Fetch all studies in storage.
   * @return {Array<Study>}
   */
  async getAll() {
    const db = await getDatabase();
    return getStore(db).getAll();
  },

  /**
   * Add a study to storage.
   * @return {Promise<void, Error>} Resolves when the study is stored, or rejects with an error.
   */
  async add(study) {
    const db = await getDatabase();
    return getStore(db).add(study);
  },

  /**
   * Remove a study from storage
   * @param recipeId The recipeId of the study to delete
   * @return {Promise<void, Error>} Resolves when the study is deleted, or rejects with an error.
   */
  async delete(recipeId) {
    const db = await getDatabase();
    return getStore(db).delete(recipeId);
  },

  /**
   * Mark a study object as having ended. Modifies the study in-place.
   * @param {IDBDatabase} db
   * @param {Study} study
   * @param {String} reason Why the study is ending.
   */
  async markAsEnded(study, reason) {
    if (reason === "unknown") {
      log.warn(`Study ${study.name} ending for unknown reason.`);
    }

    study.active = false;
    study.studyEndDate = new Date();
    const db = await getDatabase();
    await getStore(db).put(study);

    Services.obs.notifyObservers(study, STUDY_ENDED_TOPIC, `${study.recipeId}`);
    TelemetryEvents.sendEvent("unenroll", "addon_study", study.name, {
      addonId: study.addonId,
      addonVersion: study.addonVersion,
      reason,
    });

    await this.onUnenroll(study.addonId);
  },

  // Maps extension id -> Set(callbacks)
  _unenrollListeners: new Map(),

  /**
   * Register a callback to be invoked when a given study ends.
   *
   * @param {string} id         The extension id
   * @param {function} listener The callback
   */
  addUnenrollListener(id, listener) {
    let listeners = this._unenrollListeners.get(id);
    if (!listeners) {
      listeners = new Set();
      this._unenrollListeners.set(id, listeners);
    }
    listeners.add(listener);
  },

  /**
   * Invoke the unenroll callback (if any) for the given extension
   *
   * @param {string} id The extension id
   *
   * @returns {Promise} A Promise resolved after the unenroll listener
   *                    (if any) has finished its unenroll tasks.
   */
  onUnenroll(id) {
    let callbacks = this._unenrollListeners.get(id);
    let promises = [];
    if (callbacks) {
      for (let callback of callbacks) {
        promises.push(callback());
      }
    }
    return Promise.all(promises);
  },
};
