/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["SharedDataMap"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PromiseUtils",
  "resource://gre/modules/PromiseUtils.jsm"
);

const IS_MAIN_PROCESS =
  Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT;

ChromeUtils.defineModuleGetter(
  this,
  "JSONFile",
  "resource://gre/modules/JSONFile.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

class SharedDataMap extends EventEmitter {
  constructor(sharedDataKey, options = { isParent: IS_MAIN_PROCESS }) {
    super();

    this._sharedDataKey = sharedDataKey;
    this._isParent = options.isParent;
    this._isReady = false;
    this._readyDeferred = PromiseUtils.defer();
    this._data = null;

    if (this.isParent) {
      // We have an in memory store and a file backed store.
      // We use the `nonPersistentStore` for remote feature defaults and
      // `store` for experiment recipes
      this._nonPersistentStore = null;
      // Lazy-load JSON file that backs Storage instances.
      XPCOMUtils.defineLazyGetter(this, "_store", () => {
        let path = options.path;
        let store = null;
        if (!path) {
          try {
            const profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile).path;
            path = PathUtils.join(profileDir, `${sharedDataKey}.json`);
          } catch (e) {
            Cu.reportError(e);
          }
        }
        try {
          store = new JSONFile({ path });
        } catch (e) {
          Cu.reportError(e);
        }
        return store;
      });
    } else {
      this._syncFromParent();
      Services.cpmm.sharedData.addEventListener("change", this);
    }
  }

  async init() {
    if (!this._isReady && this.isParent) {
      try {
        await this._store.load();
        this._data = this._store.data;
        this._nonPersistentStore = {};
        this._syncToChildren({ flush: true });
        this._checkIfReady();
      } catch (e) {
        Cu.reportError(e);
      }
    }
  }

  get sharedDataKey() {
    return this._sharedDataKey;
  }

  get isParent() {
    return this._isParent;
  }

  ready() {
    return this._readyDeferred.promise;
  }

  get(key) {
    if (!this._data) {
      return null;
    }

    let entry = this._data[key];

    if (!entry && this._nonPersistentStore) {
      return this._nonPersistentStore[key];
    }

    return entry;
  }

  set(key, value) {
    if (!this.isParent) {
      throw new Error(
        "Setting values from within a content process is not allowed"
      );
    }
    this._store.data[key] = value;
    this._store.saveSoon();
    this._syncToChildren();
    this._notifyUpdate();
  }

  /**
   * Replace the stored data with an updated filtered dataset for cleanup
   * purposes. We don't notify of update because we're only filtering out
   * old unused entries.
   *
   * @param {string[]} keysToRemove - list of keys to remove from the persistent store
   */
  _removeEntriesByKeys(keysToRemove) {
    for (let key of keysToRemove) {
      delete this._store.data[key];
    }
    this._store.saveSoon();
  }

  setNonPersistent(key, value) {
    if (!this.isParent) {
      throw new Error(
        "Setting values from within a content process is not allowed"
      );
    }

    this._nonPersistentStore[key] = value;
    this._syncToChildren();
    this._notifyUpdate();
  }

  hasRemoteDefaultsReady() {
    return this._nonPersistentStore?.__REMOTE_DEFAULTS;
  }

  // Only used in tests
  _deleteForTests(key) {
    if (!this.isParent) {
      throw new Error(
        "Setting values from within a content process is not allowed"
      );
    }
    if (this.has(key)) {
      delete this._store.data[key];
    }
    if (this._nonPersistentStore) {
      delete this._nonPersistentStore.__REMOTE_DEFAULTS?.[key];
      if (
        !Object.keys(this._nonPersistentStore?.__REMOTE_DEFAULTS || {}).length
      ) {
        // If we are doing test cleanup and we removed all remote rollout entries
        // we want to additionally remove the __REMOTE_DEFAULTS key because
        // we use it to determine if a remote sync event happened (`.ready()`)
        this._nonPersistentStore = {};
      }
    }

    this._store.saveSoon();
    this._syncToChildren();
    this._notifyUpdate();
  }

  has(key) {
    return Boolean(this.get(key));
  }

  /**
   * Notify store listeners of updates
   * Called both from Main and Content process
   */
  _notifyUpdate(process = "parent") {
    for (let key of Object.keys(this._data || {})) {
      this.emit(`${process}-store-update:${key}`, this._data[key]);
    }
    for (let key of Object.keys(this._nonPersistentStore || {})) {
      this.emit(
        `${process}-store-update:${key}`,
        this._nonPersistentStore[key]
      );
    }
  }

  _syncToChildren({ flush = false } = {}) {
    Services.ppmm.sharedData.set(this.sharedDataKey, {
      ...this._data,
      ...this._nonPersistentStore,
    });
    if (flush) {
      Services.ppmm.sharedData.flush();
    }
  }

  _syncFromParent() {
    this._data = Services.cpmm.sharedData.get(this.sharedDataKey);
    this._checkIfReady();
    this._notifyUpdate("child");
  }

  _checkIfReady() {
    if (!this._isReady && this._data) {
      this._isReady = true;
      this._readyDeferred.resolve();
    }
  }

  handleEvent(event) {
    if (event.type === "change") {
      if (event.changedKeys.includes(this.sharedDataKey)) {
        this._syncFromParent();
      }
    }
  }
}
