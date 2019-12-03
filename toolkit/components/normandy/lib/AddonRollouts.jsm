/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

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

/**
 * AddonRollouts store info about an active or expired addon rollouts.
 * @typedef {object} AddonRollout
 * @property {int} recipeId
 *   The ID of the recipe.
 * @property {string} slug
 *   Unique slug of the rollout.
 * @property {string} state
 *   The current state of the rollout: "active", or "rolled-back".
 *   Active means that Normandy is actively managing therollout. Rolled-back
 *   means that the rollout was previously active, but has been rolled back for
 *   this user.
 * @property {int} extensionApiId
 *   The ID used to look up the extension in Normandy's API.
 * @property {string} addonId
 *   The add-on ID for this particular rollout.
 * @property {string} addonVersion
 *   The rollout add-on version number
 * @property {string} xpiUrl
 *   URL that the add-on was installed from.
 * @property {string} xpiHash
 *   The hash of the XPI file.
 * @property {string} xpiHashAlgorithm
 *   The algorithm used to hash the XPI file.
 */

var EXPORTED_SYMBOLS = ["AddonRollouts"];
const DB_NAME = "normandy-addon-rollout";
const STORE_NAME = "addon-rollouts";
const DB_OPTIONS = { version: 1 };

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

const AddonRollouts = {
  STATE_ACTIVE: "active",
  STATE_ROLLED_BACK: "rolled-back",

  async init() {
    for (const rollout of await this.getAllActive()) {
      TelemetryEnvironment.setExperimentActive(rollout.slug, rollout.state, {
        type: "normandy-addonrollout",
      });
    }
  },

  /**
   * Add a new rollout
   * @param {AddonRollout} rollout
   */
  async add(rollout) {
    const db = await getDatabase();
    return getStore(db, "readwrite").add(rollout);
  },

  /**
   * Update an existing rollout
   * @param {AddonRollout} rollout
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
   * Test wrapper that temporarily replaces the stored rollout data with fake
   * data for testing.
   */
  withTestMock(testFunction) {
    return async function inner(...args) {
      let db = await getDatabase();
      const oldData = await getStore(db, "readonly").getAll();
      await getStore(db, "readwrite").clear();
      try {
        await testFunction(...args);
      } finally {
        db = await getDatabase();
        await getStore(db, "readwrite").clear();
        const store = getStore(db, "readwrite");
        await Promise.all(oldData.map(d => store.add(d)));
      }
    };
  },
};
