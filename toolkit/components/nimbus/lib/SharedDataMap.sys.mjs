/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { EventEmitter } from "resource://gre/modules/EventEmitter.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  JSONFile: "resource://gre/modules/JSONFile.sys.mjs",
  PromiseUtils: "resource://gre/modules/PromiseUtils.sys.mjs",
});

const IS_MAIN_PROCESS =
  Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT;

export class SharedDataMap extends EventEmitter {
  constructor(sharedDataKey, options = { isParent: IS_MAIN_PROCESS }) {
    super();

    this._sharedDataKey = sharedDataKey;
    this._isParent = options.isParent;
    this._isReady = false;
    this._readyDeferred = lazy.PromiseUtils.defer();
    this._data = null;

    if (this.isParent) {
      // Lazy-load JSON file that backs Storage instances.
      ChromeUtils.defineLazyGetter(this, "_store", () => {
        let path = options.path;
        let store = null;
        if (!path) {
          try {
            const profileDir = Services.dirsvc.get("ProfD", Ci.nsIFile).path;
            path = PathUtils.join(profileDir, `${sharedDataKey}.json`);
          } catch (e) {
            console.error(e);
          }
        }
        try {
          store = new lazy.JSONFile({ path });
        } catch (e) {
          console.error(e);
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
        this._syncToChildren({ flush: true });
        this._checkIfReady();
      } catch (e) {
        console.error(e);
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
    if (!keysToRemove.length) {
      return;
    }
    for (let key of keysToRemove) {
      try {
        delete this._store.data[key];
      } catch (e) {
        // It's ok if this fails
      }
    }
    this._store.saveSoon();
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
      this._store.saveSoon();
      this._syncToChildren();
      this._notifyUpdate();
    }
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
  }

  _syncToChildren({ flush = false } = {}) {
    Services.ppmm.sharedData.set(this.sharedDataKey, {
      ...this._data,
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
