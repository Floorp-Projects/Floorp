/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionStorageEngine"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-common/async.js");
XPCOMUtils.defineLazyModuleGetter(this, "extensionStorageSync",
                                  "resource://gre/modules/ExtensionStorageSync.jsm");

/**
 * The Engine that manages syncing for the web extension "storage"
 * API, and in particular ext.storage.sync.
 *
 * ext.storage.sync is implemented using Kinto, so it has mechanisms
 * for syncing that we do not need to integrate in the Firefox Sync
 * framework, so this is something of a stub.
 */
this.ExtensionStorageEngine = function ExtensionStorageEngine(service) {
  SyncEngine.call(this, "Extension-Storage", service);
};
ExtensionStorageEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _trackerObj: ExtensionStorageTracker,
  // we don't need these since we implement our own sync logic
  _storeObj: undefined,
  _recordObj: undefined,

  syncPriority: 10,
  allowSkippedRecord: false,

  _sync() {
    return Async.promiseSpinningly(extensionStorageSync.syncAll());
  },

  get enabled() {
    // By default, we sync extension storage if we sync addons. This
    // lets us simplify the UX since users probably don't consider
    // "extension preferences" a separate category of syncing.
    // However, we also respect engine.extension-storage.force, which
    // can be set to true or false, if a power user wants to customize
    // the behavior despite the lack of UI.
    const forced = Svc.Prefs.get("engine." + this.prefName + ".force", undefined);
    if (forced !== undefined) {
      return forced;
    }
    return Svc.Prefs.get("engine.addons", false);
  },
};

function ExtensionStorageTracker(name, engine) {
  Tracker.call(this, name, engine);
}
ExtensionStorageTracker.prototype = {
  __proto__: Tracker.prototype,

  startTracking() {
    Svc.Obs.add("ext.storage.sync-changed", this);
  },

  stopTracking() {
    Svc.Obs.remove("ext.storage.sync-changed", this);
  },

  observe(subject, topic, data) {
    Tracker.prototype.observe.call(this, subject, topic, data);

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

  // Override a bunch of methods which don't do anything for us.
  // This is a performance hack.
  ignoreID() {
  },
  unignoreID() {
  },
  addChangedID() {
  },
  removeChangedID() {
  },
  clearChangedIDs() {
  },
};
