/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["SharedDataMap"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "PromiseUtils",
  "resource://gre/modules/PromiseUtils.jsm"
);

const IS_MAIN_PROCESS =
  Services.appinfo.processType === Services.appinfo.PROCESS_TYPE_DEFAULT;

class SharedDataMap {
  constructor(sharedDataKey) {
    this._sharedDataKey = sharedDataKey;
    this._isParent = IS_MAIN_PROCESS;
    this._isReady = this.isParent;
    this._readyDeferred = PromiseUtils.defer();
    this._map = null;

    if (this.isParent) {
      this._map = new Map();
      this._syncToChildren({ flush: true });
      this._checkIfReady();
    } else {
      this._syncFromParent();
      Services.cpmm.sharedData.addEventListener("change", this);
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
    return this._map.get(key);
  }

  set(key, value) {
    if (!this.isParent) {
      throw new Error(
        "Setting values from within a content process is not allowed"
      );
    }
    this._map.set(key, value);
    this._syncToChildren();
  }

  has(key) {
    return this._map.has(key);
  }

  toObject() {
    return Object.fromEntries(this._map);
  }

  _syncToChildren({ flush = false } = {}) {
    Services.ppmm.sharedData.set(this.sharedDataKey, this._map);
    if (flush) {
      Services.ppmm.sharedData.flush();
    }
  }

  _syncFromParent() {
    this._map = Services.cpmm.sharedData.get(this.sharedDataKey);
    this._checkIfReady();
  }

  _checkIfReady() {
    if (!this._isReady && this._map) {
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
