/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * @typedef {Object} Study
 * @property {Number} recipeId
 *   ID of the recipe that created the study. Used as the primary key of the
 *   study.
 * @property {Number} slug
 *   String code used to identify the study for use in Telemetry and logging.
 * @property {string} userFacingName
 *   Name of the study to show to the user
 * @property {string} userFacingDescription
 *   Description of the study and its intent.
 * @property {string} branch
 *   The branch the user is enrolled in
 * @property {boolean} active
 *   Is the study still running?
 * @property {string} addonId
 *   Add-on ID for this particular study.
 * @property {string} addonUrl
 *   URL that the study add-on was installed from.
 * @property {string} addonVersion
 *   Study add-on version number
 * @property {int} extensionApiId
 *   The ID used to look up the extension in Normandy's API.
 * @property {string} extensionHash
 *   The hash of the XPI file.
 * @property {string} extensionHashAlgorithm
 *   The algorithm used to hash the XPI file.
 * @property {Date} studyStartDate
 *   Date when the study was started.
 * @property {Date|null} studyEndDate
 *   Date when the study was ended.
 * @property {Date|null} temporaryErrorDeadline
 *   Date of when temporary errors with this experiment should no longer be
 *   considered temporary. After this point, further errors will result in
 *   unenrollment.
 */

import { LogManager } from "resource://normandy/lib/LogManager.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  BranchedAddonStudyAction:
    "resource://normandy/actions/BranchedAddonStudyAction.sys.mjs",
  CleanupManager: "resource://normandy/lib/CleanupManager.sys.mjs",
  IndexedDB: "resource://gre/modules/IndexedDB.sys.mjs",
  TelemetryEnvironment: "resource://gre/modules/TelemetryEnvironment.sys.mjs",
  TelemetryEvents: "resource://normandy/lib/TelemetryEvents.sys.mjs",
});

const DB_NAME = "shield";
const STORE_NAME = "addon-studies";
const VERSION_STORE_NAME = "addon-studies-version";
const DB_VERSION = 2;
const STUDY_ENDED_TOPIC = "shield-study-ended";
const log = LogManager.getLogger("addon-studies");

/**
 * Create a new connection to the database.
 */
function openDatabase() {
  return lazy.IndexedDB.open(DB_NAME, DB_VERSION, async (db, event) => {
    if (event.oldVersion < 1) {
      db.createObjectStore(STORE_NAME, {
        keyPath: "recipeId",
      });
    }

    if (event.oldVersion < 2) {
      db.createObjectStore(VERSION_STORE_NAME);
    }
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

export var AddonStudies = {
  /**
   * Test wrapper that temporarily replaces the stored studies with the given
   * ones. The original stored studies are restored upon completion.
   *
   * This is defined here instead of in test code since it needs to access the
   * getDatabase, which we don't expose to avoid outside modules relying on the
   * type of storage used for studies.
   *
   * @param {Array} [addonStudies=[]]
   */
  withStudies(addonStudies = []) {
    return function wrapper(testFunction) {
      return async function wrappedTestFunction(args) {
        const oldStudies = await AddonStudies.getAll();
        let db = await getDatabase();
        await AddonStudies.clear();
        const store = getStore(db, "readwrite");
        await Promise.all(addonStudies.map(study => store.add(study)));

        try {
          await testFunction({ ...args, addonStudies });
        } finally {
          db = await getDatabase();
          await AddonStudies.clear();
          const store = getStore(db, "readwrite");
          await Promise.all(oldStudies.map(study => store.add(study)));
        }
      };
    };
  },

  async init() {
    for (const study of await this.getAllActive()) {
      // If an active study's add-on has been removed since we last ran, stop it.
      const addon = await lazy.AddonManager.getAddonByID(study.addonId);
      if (!addon) {
        await this.markAsEnded(study, "uninstalled-sideload");
        continue;
      }

      // Otherwise mark that study as active in Telemetry
      lazy.TelemetryEnvironment.setExperimentActive(study.slug, study.branch, {
        type: "normandy-addonstudy",
      });
    }

    // Listen for add-on uninstalls so we can stop the corresponding studies.
    lazy.AddonManager.addAddonListener(this);
    lazy.CleanupManager.addCleanupHandler(() => {
      lazy.AddonManager.removeAddonListener(this);
    });
  },

  /**
   * These migrations should only be called from `NormandyMigrations.jsm` and
   * tests.
   */
  migrations: {
    /**
     * Change from "name" and "description" to "slug", "userFacingName",
     * and "userFacingDescription".
     */
    async migration01AddonStudyFieldsToSlugAndUserFacingFields() {
      const db = await getDatabase();
      const studies = await db.objectStore(STORE_NAME, "readonly").getAll();

      // If there are no studies, stop here to avoid opening the DB again.
      if (studies.length === 0) {
        return;
      }

      // Object stores expire after `await`, so this method accumulates a bunch of
      // promises, and then awaits them at the end.
      const writePromises = [];
      const objectStore = db.objectStore(STORE_NAME, "readwrite");

      for (const study of studies) {
        // use existing name as slug
        if (!study.slug) {
          study.slug = study.name;
        }

        // Rename `name` and `description` as `userFacingName` and `userFacingDescription`
        if (study.name && !study.userFacingName) {
          study.userFacingName = study.name;
        }
        delete study.name;
        if (study.description && !study.userFacingDescription) {
          study.userFacingDescription = study.description;
        }
        delete study.description;

        // Specify that existing recipes don't have branches
        if (!study.branch) {
          study.branch = AddonStudies.NO_BRANCHES_MARKER;
        }

        writePromises.push(objectStore.put(study));
      }

      await Promise.all(writePromises);
    },

    async migration02RemoveOldAddonStudyAction() {
      const studies = await AddonStudies.getAllActive({
        branched: AddonStudies.FILTER_NOT_BRANCHED,
      });
      if (!studies.length) {
        return;
      }
      const action = new lazy.BranchedAddonStudyAction();
      for (const study of studies) {
        try {
          await action.unenroll(
            study.recipeId,
            "migration-removing-unbranched-action"
          );
        } catch (e) {
          log.error(
            `Stopping add-on study ${study.slug} during migration failed: ${e}`
          );
        }
      }
    },
  },

  /**
   * If a study add-on is uninstalled, mark the study as having ended.
   * @param {Addon} addon
   */
  async onUninstalled(addon) {
    const activeStudies = (await this.getAll()).filter(study => study.active);
    const matchingStudy = activeStudies.find(
      study => study.addonId === addon.id
    );
    if (matchingStudy) {
      await this.markAsEnded(matchingStudy, "uninstalled");
    }
  },

  /**
   * Remove all stored studies.
   */
  async clear() {
    const db = await getDatabase();
    await getStore(db, "readwrite").clear();
  },

  /**
   * Test whether there is a study in storage for the given recipe ID.
   * @param {Number} recipeId
   * @returns {Boolean}
   */
  async has(recipeId) {
    const db = await getDatabase();
    const study = await getStore(db, "readonly").get(recipeId);
    return !!study;
  },

  /**
   * Fetch a study from storage.
   * @param {Number} recipeId
   * @return {Study} The requested study, or null if none with that ID exist.
   */
  async get(recipeId) {
    const db = await getDatabase();
    return getStore(db, "readonly").get(recipeId);
  },

  FILTER_BRANCHED_ONLY: Symbol("FILTER_BRANCHED_ONLY"),
  FILTER_NOT_BRANCHED: Symbol("FILTER_NOT_BRANCHED"),
  FILTER_ALL: Symbol("FILTER_ALL"),

  /**
   * Fetch all studies in storage.
   * @return {Array<Study>}
   */
  async getAll({ branched = AddonStudies.FILTER_ALL } = {}) {
    const db = await getDatabase();
    let results = await getStore(db, "readonly").getAll();

    if (branched == AddonStudies.FILTER_BRANCHED_ONLY) {
      results = results.filter(
        study => study.branch != AddonStudies.NO_BRANCHES_MARKER
      );
    } else if (branched == AddonStudies.FILTER_NOT_BRANCHED) {
      results = results.filter(
        study => study.branch == AddonStudies.NO_BRANCHES_MARKER
      );
    }
    return results;
  },

  /**
   * Fetch all studies in storage.
   * @return {Array<Study>}
   */
  async getAllActive(options) {
    return (await this.getAll(options)).filter(study => study.active);
  },

  /**
   * Add a study to storage.
   * @return {Promise<void, Error>} Resolves when the study is stored, or rejects with an error.
   */
  async add(study) {
    const db = await getDatabase();
    return getStore(db, "readwrite").add(study);
  },

  /**
   * Update a study in storage.
   * @return {Promise<void, Error>} Resolves when the study is updated, or rejects with an error.
   */
  async update(study) {
    const db = await getDatabase();
    return getStore(db, "readwrite").put(study);
  },

  /**
   * Update many existing studies. More efficient than calling `update` many
   * times in a row.
   * @param {Array<AddonStudy>} studies
   * @throws If any of the passed studies have a slug that doesn't exist in the database already.
   */
  async updateMany(studies) {
    // Don't touch the database if there is nothing to do
    if (!studies.length) {
      return;
    }

    // Both of the below operations use .map() instead of a normal loop becaues
    // once we get the object store, we can't let it expire by spinning the
    // event loop. This approach queues up all the interactions with the store
    // immediately, preventing it from expiring too soon.

    const db = await getDatabase();
    let store = await getStore(db, "readonly");
    await Promise.all(
      studies.map(async ({ recipeId }) => {
        let existingStudy = await store.get(recipeId);
        if (!existingStudy) {
          throw new Error(
            `Tried to update addon study ${recipeId}, but it doesn't exist.`
          );
        }
      })
    );

    // awaiting spun the event loop, so the store is now invalid. Get a new
    // store. This is also a chance to get it in readwrite mode.
    store = await getStore(db, "readwrite");
    await Promise.all(studies.map(study => store.put(study)));
  },

  /**
   * Remove a study from storage
   * @param recipeId The recipeId of the study to delete
   * @return {Promise<void, Error>} Resolves when the study is deleted, or rejects with an error.
   */
  async delete(recipeId) {
    const db = await getDatabase();
    return getStore(db, "readwrite").delete(recipeId);
  },

  /**
   * Mark a study object as having ended. Modifies the study in-place.
   * @param {IDBDatabase} db
   * @param {Study} study
   * @param {String} reason Why the study is ending.
   */
  async markAsEnded(study, reason = "unknown") {
    if (reason === "unknown") {
      log.warn(`Study ${study.slug} ending for unknown reason.`);
    }

    study.active = false;
    study.temporaryErrorDeadline = null;
    study.studyEndDate = new Date();
    const db = await getDatabase();
    await getStore(db, "readwrite").put(study);

    Services.obs.notifyObservers(study, STUDY_ENDED_TOPIC, `${study.recipeId}`);
    lazy.TelemetryEvents.sendEvent("unenroll", "addon_study", study.slug, {
      addonId: study.addonId || AddonStudies.NO_ADDON_MARKER,
      addonVersion: study.addonVersion || AddonStudies.NO_ADDON_MARKER,
      reason,
      branch: study.branch,
    });
    lazy.TelemetryEnvironment.setExperimentInactive(study.slug);

    await this.callUnenrollListeners(study.addonId, reason);
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
   * Unregister a callback to be invoked when a given study ends.
   *
   * @param {string} id         The extension id
   * @param {function} listener The callback
   */
  removeUnenrollListener(id, listener) {
    let listeners = this._unenrollListeners.get(id);
    if (listeners) {
      listeners.delete(listener);
    }
  },

  /**
   * Invoke the unenroll callback (if any) for the given extension
   *
   * @param {string} id The extension id
   * @param {string} reason Why the study is ending
   *
   * @returns {Promise} A Promise resolved after the unenroll listener
   *                    (if any) has finished its unenroll tasks.
   */
  async callUnenrollListeners(id, reason) {
    let callbacks = this._unenrollListeners.get(id) || [];

    async function callCallback(cb, reason) {
      try {
        await cb(reason);
      } catch (err) {
        console.error(err);
      }
    }

    let promises = [];
    for (let callback of callbacks) {
      promises.push(callCallback(callback, reason));
    }

    // Wait for all the promises to be settled. This won't throw even if some of
    // the listeners fail.
    await Promise.all(promises);
  },
};

AddonStudies.NO_BRANCHES_MARKER = "__NO_BRANCHES__";
AddonStudies.NO_ADDON_MARKER = "__NO_ADDON__";
