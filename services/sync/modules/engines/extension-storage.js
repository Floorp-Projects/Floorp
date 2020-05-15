/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "ExtensionStorageEngineKinto",
  "ExtensionStorageEngineBridge",
];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  BridgedEngine: "resource://services-sync/bridged_engine.js",
  extensionStorageSync: "resource://gre/modules/ExtensionStorageSyncKinto.jsm",
  Svc: "resource://services-sync/util.js",
  SyncEngine: "resource://services-sync/engines.js",
  Tracker: "resource://services-sync/engines.js",
  SCORE_INCREMENT_MEDIUM: "resource://services-sync/constants.js",
  MULTI_DEVICE_THRESHOLD: "resource://services-sync/constants.js",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "StorageSyncService",
  "@mozilla.org/extensions/storage/sync;1",
  "nsIInterfaceRequestor"
);

// A helper to indicate whether extension-storage is enabled - it's based on
// the "addons" pref. The same logic is shared between both engine impls.
function isEngineEnabled() {
  // By default, we sync extension storage if we sync addons. This
  // lets us simplify the UX since users probably don't consider
  // "extension preferences" a separate category of syncing.
  // However, we also respect engine.extension-storage.force, which
  // can be set to true or false, if a power user wants to customize
  // the behavior despite the lack of UI.
  const forced = Svc.Prefs.get("engine.extension-storage.force", undefined);
  if (forced !== undefined) {
    return forced;
  }
  return Svc.Prefs.get("engine.addons", false);
}

// A "bridged engine" to our webext-storage component.
function ExtensionStorageEngineBridge(service) {
  let bridge = StorageSyncService.getInterface(Ci.mozIBridgedSyncEngine);
  BridgedEngine.call(this, bridge, "Extension-Storage", service);
}

ExtensionStorageEngineBridge.prototype = {
  __proto__: BridgedEngine.prototype,
  syncPriority: 10,
  // we don't support repair at all!
  _skipPercentageChance: 100,

  get enabled() {
    return isEngineEnabled();
  },
};

/**
 *****************************************************************************
 *
 * Deprecated support for Kinto
 *
 *****************************************************************************
 */

/**
 * The Engine that manages syncing for the web extension "storage"
 * API, and in particular ext.storage.sync.
 *
 * ext.storage.sync is implemented using Kinto, so it has mechanisms
 * for syncing that we do not need to integrate in the Firefox Sync
 * framework, so this is something of a stub.
 */
function ExtensionStorageEngineKinto(service) {
  SyncEngine.call(this, "Extension-Storage", service);
  XPCOMUtils.defineLazyPreferenceGetter(
    this,
    "_skipPercentageChance",
    "services.sync.extension-storage.skipPercentageChance",
    0
  );
}
ExtensionStorageEngineKinto.prototype = {
  __proto__: SyncEngine.prototype,
  _trackerObj: ExtensionStorageTracker,
  // we don't need these since we implement our own sync logic
  _storeObj: undefined,
  _recordObj: undefined,

  syncPriority: 10,
  allowSkippedRecord: false,

  async _sync() {
    return extensionStorageSync.syncAll();
  },

  get enabled() {
    return isEngineEnabled();
  },

  _wipeClient() {
    return extensionStorageSync.clearAll();
  },

  shouldSkipSync(syncReason) {
    if (syncReason == "user" || syncReason == "startup") {
      this._log.info(
        `Not skipping extension storage sync: reason == ${syncReason}`
      );
      // Always sync if a user clicks the button, or if we're starting up.
      return false;
    }
    // Ensure this wouldn't cause a resync...
    if (this._tracker.score >= MULTI_DEVICE_THRESHOLD) {
      this._log.info(
        "Not skipping extension storage sync: Would trigger resync anyway"
      );
      return false;
    }

    let probability = this._skipPercentageChance / 100.0;
    // Math.random() returns a value in the interval [0, 1), so `>` is correct:
    // if `probability` is 1 skip every time, and if it's 0, never skip.
    let shouldSkip = probability > Math.random();

    this._log.info(
      `Skipping extension-storage sync with a chance of ${probability}: ${shouldSkip}`
    );
    return shouldSkip;
  },
};

function ExtensionStorageTracker(name, engine) {
  Tracker.call(this, name, engine);
  this._ignoreAll = false;
}
ExtensionStorageTracker.prototype = {
  __proto__: Tracker.prototype,

  get ignoreAll() {
    return this._ignoreAll;
  },

  set ignoreAll(value) {
    this._ignoreAll = value;
  },

  onStart() {
    Svc.Obs.add("ext.storage.sync-changed", this.asyncObserver);
  },

  onStop() {
    Svc.Obs.remove("ext.storage.sync-changed", this.asyncObserver);
  },

  async observe(subject, topic, data) {
    if (this.ignoreAll) {
      return;
    }

    if (topic !== "ext.storage.sync-changed") {
      return;
    }

    // Single adds, removes and changes are not so important on their
    // own, so let's just increment score a bit.
    this.score += SCORE_INCREMENT_MEDIUM;
  },
};
