/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "JSONFile",
  "resource://gre/modules/JSONFile.jsm"
);
ChromeUtils.defineModuleGetter(lazy, "OS", "resource://gre/modules/osfile.jsm");

var EXPORTED_SYMBOLS = ["Storage"];

// Lazy-load JSON file that backs Storage instances.
XPCOMUtils.defineLazyGetter(lazy, "lazyStore", async function() {
  const path = lazy.OS.Path.join(
    lazy.OS.Constants.Path.profileDir,
    "shield-recipe-client.json"
  );
  const store = new lazy.JSONFile({ path });
  await store.load();
  return store;
});

var Storage = class {
  constructor(prefix) {
    this.prefix = prefix;
  }

  /**
   * Clear ALL storage data and save to the disk.
   */
  static async clearAllStorage() {
    const store = await lazy.lazyStore;
    store.data = {};
    store.saveSoon();
  }

  /**
   * Sets an item in the prefixed storage.
   * @returns {Promise}
   * @resolves With the stored value, or null.
   * @rejects Javascript exception.
   */
  async getItem(name) {
    const store = await lazy.lazyStore;
    const namespace = store.data[this.prefix] || {};
    return namespace[name] || null;
  }

  /**
   * Sets an item in the prefixed storage.
   * @returns {Promise}
   * @resolves When the operation is completed successfully
   * @rejects Javascript exception.
   */
  async setItem(name, value) {
    const store = await lazy.lazyStore;
    if (!(this.prefix in store.data)) {
      store.data[this.prefix] = {};
    }
    store.data[this.prefix][name] = value;
    store.saveSoon();
  }

  /**
   * Removes a single item from the prefixed storage.
   * @returns {Promise}
   * @resolves When the operation is completed successfully
   * @rejects Javascript exception.
   */
  async removeItem(name) {
    const store = await lazy.lazyStore;
    if (this.prefix in store.data) {
      delete store.data[this.prefix][name];
      store.saveSoon();
    }
  }

  /**
   * Clears all storage for the prefix.
   * @returns {Promise}
   * @resolves When the operation is completed successfully
   * @rejects Javascript exception.
   */
  async clear() {
    const store = await lazy.lazyStore;
    store.data[this.prefix] = {};
    store.saveSoon();
  }
};
