/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const STORAGE_SYNC_ENABLED_PREF = "webextensions.storage.sync.enabled";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const NS_ERROR_DOM_QUOTA_EXCEEDED_ERR = 0x80530016;

/** @type {Lazy} */
const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ExtensionCommon: "resource://gre/modules/ExtensionCommon.sys.mjs",
  ExtensionUtils: "resource://gre/modules/ExtensionUtils.sys.mjs",
  // We might end up falling back to kinto...
  extensionStorageSyncKinto:
    "resource://gre/modules/ExtensionStorageSyncKinto.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "prefPermitsStorageSync",
  STORAGE_SYNC_ENABLED_PREF,
  true
);

// This xpcom service implements a "bridge" from the JS world to the Rust world.
// It sets up the database and implements a callback-based version of the
// browser.storage API.
ChromeUtils.defineLazyGetter(lazy, "storageSvc", () =>
  Cc["@mozilla.org/extensions/storage/sync;1"]
    .getService(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.mozIExtensionStorageArea)
);

// The interfaces which define the callbacks used by the bridge. There's a
// callback for success, failure, and to record data changes.
function ExtensionStorageApiCallback(resolve, reject, changeCallback) {
  this.resolve = resolve;
  this.reject = reject;
  this.changeCallback = changeCallback;
}

ExtensionStorageApiCallback.prototype = {
  QueryInterface: ChromeUtils.generateQI([
    "mozIExtensionStorageListener",
    "mozIExtensionStorageCallback",
  ]),

  handleSuccess(result) {
    this.resolve(result ? JSON.parse(result) : null);
  },

  handleError(code, message) {
    /** @type {Error & { code?: number }} */
    let e = new Error(message);
    e.code = code;
    Cu.reportError(e);
    this.reject(e);
  },

  onChanged(extId, json) {
    if (this.changeCallback && json) {
      try {
        this.changeCallback(extId, JSON.parse(json));
      } catch (ex) {
        Cu.reportError(ex);
      }
    }
  },
};

// The backing implementation of the browser.storage.sync web extension API.
export class ExtensionStorageSync {
  constructor() {
    this.listeners = new Map();
    // We are optimistic :) If we ever see the special nsresult which indicates
    // migration failure, it will become false. In practice, this will only ever
    // happen on the first operation.
    this.migrationOk = true;
  }

  // The main entry-point to our bridge. It performs some important roles:
  // * Ensures the API is allowed to be used.
  // * Works out what "extension id" to use.
  // * Turns the callback API into a promise API.
  async _promisify(fnName, extension, context, ...args) {
    let extId = extension.id;
    if (lazy.prefPermitsStorageSync !== true) {
      throw new lazy.ExtensionUtils.ExtensionError(
        `Please set ${STORAGE_SYNC_ENABLED_PREF} to true in about:config`
      );
    }

    if (this.migrationOk) {
      // We can call ours.
      try {
        return await new Promise((resolve, reject) => {
          let callback = new ExtensionStorageApiCallback(
            resolve,
            reject,
            (extId, changes) => this.notifyListeners(extId, changes)
          );
          let sargs = args.map(val => JSON.stringify(val));
          lazy.storageSvc[fnName](extId, ...sargs, callback);
        });
      } catch (ex) {
        if (ex.code != Cr.NS_ERROR_CANNOT_CONVERT_DATA) {
          // Some non-migration related error we want to sanitize and propagate.
          // The only "public" exception here is for quota failure - all others
          // are sanitized.
          let sanitized =
            ex.code == NS_ERROR_DOM_QUOTA_EXCEEDED_ERR
              ? // The same message as the local IDB implementation
                `QuotaExceededError: storage.sync API call exceeded its quota limitations.`
              : // The standard, generic extension error.
                "An unexpected error occurred";
          throw new lazy.ExtensionUtils.ExtensionError(sanitized);
        }
        // This means "migrate failed" so we must fall back to kinto.
        Cu.reportError(
          "migration of extension-storage failed - will fall back to kinto"
        );
        this.migrationOk = false;
      }
    }
    // We've detected failure to migrate, so we want to use kinto.
    return lazy.extensionStorageSyncKinto[fnName](extension, ...args, context);
  }

  set(extension, items, context) {
    return this._promisify("set", extension, context, items);
  }

  remove(extension, keys, context) {
    return this._promisify("remove", extension, context, keys);
  }

  clear(extension, context) {
    return this._promisify("clear", extension, context);
  }

  clearOnUninstall(extensionId) {
    if (!this.migrationOk) {
      // If the rust-based backend isn't being used,
      // no need to clear it.
      return;
    }
    // Resolve the returned promise once the request has been either resolved
    // or rejected (and report the error on the browser console in case of
    // unexpected clear failures on addon uninstall).
    return new Promise(resolve => {
      const callback = new ExtensionStorageApiCallback(
        resolve,
        err => {
          Cu.reportError(err);
          resolve();
        },
        // empty changeCallback (no need to notify the extension
        // while clearing the extension on uninstall).
        () => {}
      );
      lazy.storageSvc.clear(extensionId, callback);
    });
  }

  get(extension, spec, context) {
    return this._promisify("get", extension, context, spec);
  }

  getBytesInUse(extension, keys, context) {
    return this._promisify("getBytesInUse", extension, context, keys);
  }

  addOnChangedListener(extension, listener) {
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
        lazy.ExtensionCommon.runSafeSyncWithoutClone(listener, changes);
      }
    }
  }
}

export var extensionStorageSync = new ExtensionStorageSync();
