/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "ExtensionStorageEngineKinto",
  "ExtensionStorageEngineBridge",
];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { BridgedEngine, BridgeWrapperXPCOM } = ChromeUtils.import(
  "resource://services-sync/bridged_engine.js"
);
const { SyncEngine } = ChromeUtils.import(
  "resource://services-sync/engines.js"
);
const { Tracker } = ChromeUtils.import("resource://services-sync/engines.js");

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  LogAdapter: "resource://services-sync/bridged_engine.js",
  extensionStorageSync: "resource://gre/modules/ExtensionStorageSync.jsm",
  extensionStorageSyncKinto:
    "resource://gre/modules/ExtensionStorageSyncKinto.jsm",
  Observers: "resource://services-common/observers.js",
  Svc: "resource://services-sync/util.js",
  SCORE_INCREMENT_MEDIUM: "resource://services-sync/constants.js",
  MULTI_DEVICE_THRESHOLD: "resource://services-sync/constants.js",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "StorageSyncService",
  "@mozilla.org/extensions/storage/sync;1",
  "nsIInterfaceRequestor"
);

const PREF_FORCE_ENABLE = "engine.extension-storage.force";

// A helper to indicate whether extension-storage is enabled - it's based on
// the "addons" pref. The same logic is shared between both engine impls.
function getEngineEnabled() {
  // By default, we sync extension storage if we sync addons. This
  // lets us simplify the UX since users probably don't consider
  // "extension preferences" a separate category of syncing.
  // However, we also respect engine.extension-storage.force, which
  // can be set to true or false, if a power user wants to customize
  // the behavior despite the lack of UI.
  const forced = lazy.Svc.Prefs.get(PREF_FORCE_ENABLE, undefined);
  if (forced !== undefined) {
    return forced;
  }
  return lazy.Svc.Prefs.get("engine.addons", false);
}

function setEngineEnabled(enabled) {
  // This will be called by the engine manager when declined on another device.
  // Things will go a bit pear-shaped if the engine manager tries to end up
  // with 'addons' and 'extension-storage' in different states - however, this
  // *can* happen given we support the `engine.extension-storage.force`
  // preference. So if that pref exists, we set it to this value. If that pref
  // doesn't exist, we just ignore it and hope that the 'addons' engine is also
  // going to be set to the same state.
  if (lazy.Svc.Prefs.has(PREF_FORCE_ENABLE)) {
    lazy.Svc.Prefs.set(PREF_FORCE_ENABLE, enabled);
  }
}

// A "bridged engine" to our webext-storage component.
function ExtensionStorageEngineBridge(service) {
  this.component = lazy.StorageSyncService.getInterface(
    Ci.mozIBridgedSyncEngine
  );
  BridgedEngine.call(this, "Extension-Storage", service);
  this._bridge = new BridgeWrapperXPCOM(this.component);

  let app_services_logger = Cc["@mozilla.org/appservices/logger;1"].getService(
    Ci.mozIAppServicesLogger
  );
  let logger_target = "app-services:webext_storage:sync";
  app_services_logger.register(logger_target, new lazy.LogAdapter(this._log));
}

ExtensionStorageEngineBridge.prototype = {
  __proto__: BridgedEngine.prototype,
  syncPriority: 10,

  // Used to override the engine name in telemetry, so that we can distinguish .
  overrideTelemetryName: "rust-webext-storage",

  _notifyPendingChanges() {
    return new Promise(resolve => {
      this.component
        .QueryInterface(Ci.mozISyncedExtensionStorageArea)
        .fetchPendingSyncChanges({
          QueryInterface: ChromeUtils.generateQI([
            "mozIExtensionStorageListener",
            "mozIExtensionStorageCallback",
          ]),
          onChanged: (extId, json) => {
            try {
              lazy.extensionStorageSync.notifyListeners(
                extId,
                JSON.parse(json)
              );
            } catch (ex) {
              this._log.warn(
                `Error notifying change listeners for ${extId}`,
                ex
              );
            }
          },
          handleSuccess: resolve,
          handleError: (code, message) => {
            this._log.warn(
              "Error fetching pending synced changes",
              message,
              code
            );
            resolve();
          },
        });
    });
  },

  _takeMigrationInfo() {
    return new Promise((resolve, reject) => {
      this.component
        .QueryInterface(Ci.mozIExtensionStorageArea)
        .takeMigrationInfo({
          QueryInterface: ChromeUtils.generateQI([
            "mozIExtensionStorageCallback",
          ]),
          handleSuccess: result => {
            resolve(result ? JSON.parse(result) : null);
          },
          handleError: (code, message) => {
            this._log.warn("Error fetching migration info", message, code);
            // `takeMigrationInfo` doesn't actually perform the migration,
            // just reads (and clears) any data stored in the DB from the
            // previous migration.
            //
            // Any errors here are very likely occurring a good while
            // after the migration ran, so we just warn and pretend
            // nothing was there.
            resolve(null);
          },
        });
    });
  },

  async _syncStartup() {
    let result = await super._syncStartup();
    let info = await this._takeMigrationInfo();
    if (info) {
      lazy.Observers.notify(
        "weave:telemetry:migration",
        info,
        "webext-storage"
      );
    }
    return result;
  },

  async _processIncoming() {
    await super._processIncoming();
    try {
      await this._notifyPendingChanges();
    } catch (ex) {
      // Failing to notify `storage.onChanged` observers is bad, but shouldn't
      // interrupt syncing.
      this._log.warn("Error notifying about synced changes", ex);
    }
  },

  get enabled() {
    return getEngineEnabled();
  },
  set enabled(enabled) {
    setEngineEnabled(enabled);
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
    return lazy.extensionStorageSyncKinto.syncAll();
  },

  get enabled() {
    return getEngineEnabled();
  },
  // We only need the enabled setter for the edge-case where info/collections
  // has `extension-storage` - which could happen if the pref to flip the new
  // engine on was once set but no longer is.
  set enabled(enabled) {
    setEngineEnabled(enabled);
  },

  _wipeClient() {
    return lazy.extensionStorageSyncKinto.clearAll();
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
    if (this._tracker.score >= lazy.MULTI_DEVICE_THRESHOLD) {
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
    lazy.Svc.Obs.add("ext.storage.sync-changed", this.asyncObserver);
  },

  onStop() {
    lazy.Svc.Obs.remove("ext.storage.sync-changed", this.asyncObserver);
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
    this.score += lazy.SCORE_INCREMENT_MEDIUM;
  },
};
