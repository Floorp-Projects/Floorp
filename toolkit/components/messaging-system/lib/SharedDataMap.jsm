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
ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");

class SharedDataMap extends EventEmitter {
  constructor(sharedDataKey, options = { isParent: IS_MAIN_PROCESS }) {
    super();

    this._sharedDataKey = sharedDataKey;
    this._isParent = options.isParent;
    this._isReady = false;
    this._readyDeferred = PromiseUtils.defer();
    this._data = null;

    if (this.isParent) {
      // Lazy-load JSON file that backs Storage instances.
      XPCOMUtils.defineLazyGetter(this, "_store", () => {
        const path =
          options.path || // Only used in tests
          OS.Path.join(OS.Constants.Path.profileDir, `${sharedDataKey}.json`);
        const store = new JSONFile({ path });
        return store;
      });
    } else {
      this._syncFromParent();
      Services.cpmm.sharedData.addEventListener("change", this);
    }
  }

  async init(runSync = false) {
    if (!this._isReady && this.isParent) {
      if (runSync) {
        this._store.ensureDataReady();
      } else {
        await this._store.load();
      }
      this._data = this._store.data;
      this._syncToChildren({ flush: true });
      this._checkIfReady();
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
    return this._data[key];
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
    return Boolean(this._data[key]);
  }

  /**
   * Notify store listeners of updates
   * Called both from Main and Content process
   */
  _notifyUpdate() {
    for (let key of Object.keys(this._data)) {
      this.emit(`update:${key}`, this._data[key]);
    }
  }

  _syncToChildren({ flush = false } = {}) {
    Services.ppmm.sharedData.set(this.sharedDataKey, this._data);
    if (flush) {
      Services.ppmm.sharedData.flush();
    }
  }

  _syncFromParent() {
    this._data = Services.cpmm.sharedData.get(this.sharedDataKey);
    this._checkIfReady();
    this._notifyUpdate();
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
