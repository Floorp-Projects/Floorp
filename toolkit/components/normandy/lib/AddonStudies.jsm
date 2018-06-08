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
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "FileUtils", "resource://gre/modules/FileUtils.jsm");
ChromeUtils.defineModuleGetter(this, "IndexedDB", "resource://gre/modules/IndexedDB.jsm");
ChromeUtils.defineModuleGetter(this, "AddonManager", "resource://gre/modules/AddonManager.jsm");
ChromeUtils.defineModuleGetter(this, "Addons", "resource://normandy/lib/Addons.jsm");
ChromeUtils.defineModuleGetter(
  this, "CleanupManager", "resource://normandy/lib/CleanupManager.jsm"
);
ChromeUtils.defineModuleGetter(this, "LogManager", "resource://normandy/lib/LogManager.jsm");
ChromeUtils.defineModuleGetter(this, "TelemetryEvents", "resource://normandy/lib/TelemetryEvents.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]); /* globals fetch */

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

/**
 * Mark a study object as having ended. Modifies the study in-place.
 * @param {IDBDatabase} db
 * @param {Study} study
 * @param {String} reason Why the study is ending.
 */
async function markAsEnded(db, study, reason) {
  if (reason === "unknown") {
    log.warn(`Study ${study.name} ending for unknown reason.`);
  }

  study.active = false;
  study.studyEndDate = new Date();
  await getStore(db).put(study);

  Services.obs.notifyObservers(study, STUDY_ENDED_TOPIC, `${study.recipeId}`);
  TelemetryEvents.sendEvent("unenroll", "addon_study", study.name, {
    addonId: study.addonId,
    addonVersion: study.addonVersion,
    reason,
  });
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
    const db = await getDatabase();
    for (const study of activeStudies) {
      const addon = await AddonManager.getAddonByID(study.addonId);
      if (!addon) {
        await markAsEnded(db, study, "uninstalled-sideload");
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
      await markAsEnded(db, matchingStudy, "uninstalled");
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
   * Start a new study. Installs an add-on and stores the study info.
   * @param {Object} options
   * @param {Number} options.recipeId
   * @param {String} options.name
   * @param {String} options.description
   * @param {String} options.addonUrl
   * @throws
   *   If any of the required options aren't given.
   *   If a study for the given recipeID already exists in storage.
   *   If add-on installation fails.
   */
  async start({recipeId, name, description, addonUrl}) {
    if (!recipeId || !name || !description || !addonUrl) {
      throw new Error("Required arguments (recipeId, name, description, addonUrl) missing.");
    }

    const db = await getDatabase();
    if (await getStore(db).get(recipeId)) {
      throw new Error(`A study for recipe ${recipeId} already exists.`);
    }

    let addonFile;
    try {
      addonFile = await this.downloadAddonToTemporaryFile(addonUrl);
      const install = await AddonManager.getInstallForFile(addonFile);
      const study = {
        recipeId,
        name,
        description,
        addonId: install.addon.id,
        addonVersion: install.addon.version,
        addonUrl,
        active: true,
        studyStartDate: new Date(),
      };

      await getStore(db).add(study);
      await Addons.applyInstall(install, false);

      TelemetryEvents.sendEvent("enroll", "addon_study", name, {
        addonId: install.addon.id,
        addonVersion: install.addon.version,
      });

      return study;
    } catch (err) {
      await getStore(db).delete(recipeId);

      // The actual stack trace and error message could possibly
      // contain PII, so we don't include them here. Instead include
      // some information that should still be helpful, and is less
      // likely to be unsafe.
      const safeErrorMessage = `${err.fileName}:${err.lineNumber}:${err.columnNumber} ${err.name}`;
      TelemetryEvents.sendEvent("enrollFailed", "addon_study", name, {
        reason: safeErrorMessage.slice(0, 80),  // max length is 80 chars
      });

      throw err;
    } finally {
      if (addonFile) {
        Services.obs.notifyObservers(addonFile, "flush-cache-entry");
        await OS.File.remove(addonFile.path);
      }
    }
  },

  /**
   * Download a remote add-on and store it in a temporary nsIFile.
   * @param {String} addonUrl
   * @returns {nsIFile}
   */
  async downloadAddonToTemporaryFile(addonUrl) {
    const response = await fetch(addonUrl);
    if (!response.ok) {
      throw new Error(`Download for ${addonUrl} failed: ${response.status} ${response.statusText}`);
    }

    // Create temporary file to store add-on.
    const path = OS.Path.join(OS.Constants.Path.tmpDir, "study.xpi");
    const {file, path: uniquePath} = await OS.File.openUnique(path);

    // Write the add-on to the file
    try {
      const xpiArrayBufferView = new Uint8Array(await response.arrayBuffer());
      await file.write(xpiArrayBufferView);
    } finally {
      await file.close();
    }

    return new FileUtils.File(uniquePath);
  },

  /**
   * Stop an active study, uninstalling the associated add-on.
   * @param {Number} recipeId
   * @param {String} reason Why the study is ending. Optional, defaults to "unknown".
   * @throws
   *   If no study is found with the given recipeId.
   *   If the study is already inactive.
   */
  async stop(recipeId, reason = "unknown") {
    const db = await getDatabase();
    const study = await getStore(db).get(recipeId);
    if (!study) {
      throw new Error(`No study found for recipe ${recipeId}.`);
    }
    if (!study.active) {
      throw new Error(`Cannot stop study for recipe ${recipeId}; it is already inactive.`);
    }

    await markAsEnded(db, study, reason);

    try {
      await Addons.uninstall(study.addonId);
    } catch (err) {
      log.warn(`Could not uninstall addon ${study.addonId} for recipe ${study.recipeId}:`, err);
    }
  },
};
