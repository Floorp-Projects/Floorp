/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
});

const EXPORTED_SYMBOLS = ["StorageSyncService"];

const StorageSyncArea = Components.Constructor(
  "@mozilla.org/extensions/storage/internal/sync-area;1",
  "mozIConfigurableExtensionStorageArea",
  "configure"
);

/**
 * An XPCOM service for the WebExtension `storage.sync` API. The service manages
 * a storage area for storing and syncing extension data.
 *
 * The service configures its storage area with the database path, and hands
 * out references to the configured area via `getInterface`. It also registers
 * a shutdown blocker to automatically tear down the area.
 *
 * ## What's the difference between `storage/internal/storage-sync-area;1` and
 *    `storage/sync;1`?
 *
 * `components.conf` has two classes:
 * `@mozilla.org/extensions/storage/internal/sync-area;1` and
 * `@mozilla.org/extensions/storage/sync;1`.
 *
 * The `storage/internal/sync-area;1` class is implemented in Rust, and can be
 * instantiated using `createInstance` and `Components.Constructor`. It's not
 * a singleton, so creating a new instance will create a new `storage.sync`
 * area, with its own database connection. It's useful for testing, but not
 * meant to be used outside of this module.
 *
 * The `storage/sync;1` class is implemented in this file. It's a singleton,
 * ensuring there's only one `storage.sync` area, with one database connection.
 * The service implements `nsIInterfaceRequestor`, so callers can access the
 * storage interface like this:
 *
 *     let storageSyncArea = Cc["@mozilla.org/extensions/storage/sync;1"]
 *        .getService(Ci.nsIInterfaceRequestor)
 *        .getInterface(Ci.mozIExtensionStorageArea);
 *
 * ...And the Sync interface like this:
 *
 *    let extensionStorageEngine = Cc["@mozilla.org/extensions/storage/sync;1"]
 *       .getService(Ci.nsIInterfaceRequestor)
 *       .getInterface(Ci.mozIBridgedSyncEngine);
 *
 * @class
 */
function StorageSyncService() {
  if (StorageSyncService._singleton) {
    return StorageSyncService._singleton;
  }

  let file = FileUtils.getFile("ProfD", ["storage-sync2.sqlite"]);
  this._storageArea = new StorageSyncArea(file);

  // Register a blocker to close the storage connection on shutdown.
  this._shutdownBound = () => this._shutdown();
  AsyncShutdown.profileChangeTeardown.addBlocker(
    "StorageSyncService: shutdown",
    this._shutdownBound
  );

  StorageSyncService._singleton = this;
}

StorageSyncService._singleton = null;

StorageSyncService.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIInterfaceRequestor]),

  // Returns the storage and syncing interfaces. This just hands out a
  // reference to the underlying storage area, with a quick check to make sure
  // that callers are asking for the right interfaces.
  getInterface(iid) {
    if (iid.equals(Ci.mozIExtensionStorageArea)) {
      return this._storageArea.QueryInterface(iid);
    }
    throw Components.Exception(
      "This interface isn't implemented",
      Cr.NS_ERROR_NO_INTERFACE
    );
  },

  // Tears down the storage area and lifts the blocker so that shutdown can
  // continue.
  async _shutdown() {
    try {
      await new Promise((resolve, reject) => {
        this._storageArea.teardown({
          handleSuccess: resolve,
          handleError(code, message) {
            reject(Components.Exception(message, code));
          },
        });
      });
    } finally {
      AsyncShutdown.profileChangeTeardown.removeBlocker(this._shutdownBound);
    }
  },
};
