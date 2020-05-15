/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ExtensionStorageSync", "extensionStorageSync"];

const STORAGE_SYNC_ENABLED_PREF = "webextensions.storage.sync.enabled";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ExtensionCommon: "resource://gre/modules/ExtensionCommon.jsm",
  ExtensionUtils: "resource://gre/modules/ExtensionUtils.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "prefPermitsStorageSync",
  STORAGE_SYNC_ENABLED_PREF,
  true
);

// This xpcom service implements a "bridge" from the JS world to the Rust world.
// It sets up the database and implements a callback-based version of the
// browser.storage API.
XPCOMUtils.defineLazyGetter(this, "storageSvc", () =>
  Cc["@mozilla.org/extensions/storage/sync;1"]
    .getService(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.mozIExtensionStorageArea)
);

// The interfaces which define the callbacks used by the bridge. There's a
// callback for success, failure, and to record data changes.
function ExtensionStorageApiCallback(resolve, reject, extId, changeCallback) {
  this.resolve = resolve;
  this.reject = reject;
  this.extId = extId;
  this.changeCallback = changeCallback;
}

ExtensionStorageApiCallback.prototype = {
  QueryInterface: ChromeUtils.generateQI([
    Ci.mozIExtensionStorageListener,
    Ci.mozIExtensionStorageCallback,
  ]),

  handleSuccess(result) {
    this.resolve(result ? JSON.parse(result) : null);
  },

  handleError(code, message) {
    let e = new Error(message);
    e.code = code;
    Cu.reportError(e);
    // We always sanitize the error returned to extensions.
    this.reject(
      new ExtensionUtils.ExtensionError("An unexpected error occurred")
    );
  },

  onChanged(json) {
    if (this.changeCallback && json) {
      try {
        this.changeCallback(this.extId, JSON.parse(json));
      } catch (ex) {
        Cu.reportError(ex);
      }
    }
  },
};

// The backing implementation of the browser.storage.sync web extension API.
class ExtensionStorageSync {
  constructor() {
    this.listeners = new Map();
  }

  // The main entry-point to our bridge. It performs some important roles:
  // * Ensures the API is allowed to be used.
  // * Works out what "extension id" to use.
  // * Turns the callback API into a promise API.
  async _promisify(fn, extension, ...args) {
    let extId = extension.id;
    if (prefPermitsStorageSync !== true) {
      throw new ExtensionUtils.ExtensionError(
        `Please set ${STORAGE_SYNC_ENABLED_PREF} to true in about:config`
      );
    }
    return new Promise((resolve, reject) => {
      let callback = new ExtensionStorageApiCallback(
        resolve,
        reject,
        extId,
        (extId, changes) => this.notifyListeners(extId, changes)
      );
      fn(extId, ...args, callback);
    });
  }

  set(extension, items, context) {
    return this._promisify(storageSvc.set, extension, JSON.stringify(items));
  }

  remove(extension, keys, context) {
    return this._promisify(storageSvc.remove, extension, JSON.stringify(keys));
  }

  clear(extension, context) {
    return this._promisify(storageSvc.clear, extension);
  }

  get(extension, spec, context) {
    return this._promisify(storageSvc.get, extension, JSON.stringify(spec));
  }

  addOnChangedListener(extension, listener, context) {
    let listeners = this.listeners.get(extension.id) || new Set();
    listeners.add(listener);
    this.listeners.set(extension.id, listeners);
  }

  removeOnChangedListener(extension, listener) {
    let listeners = this.listeners.get(extension.id);
    listeners.delete(listener);
    if (listeners.size == 0) {
      this.listeners.delete(extension.id);
    }
  }

  notifyListeners(extId, changes) {
    let listeners = this.listeners.get(extId) || new Set();
    if (listeners) {
      for (let listener of listeners) {
        ExtensionCommon.runSafeSyncWithoutClone(listener, changes);
      }
    }
  }
}

var extensionStorageSync = new ExtensionStorageSync();
