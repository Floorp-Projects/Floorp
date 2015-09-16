/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "RotaryEngine",
  "RotaryRecord",
  "RotaryStore",
  "RotaryTracker",
];

const {utils: Cu} = Components;

Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/record.js");
Cu.import("resource://services-sync/util.js");

/*
 * A fake engine implementation.
 * This is used all over the place.
 *
 * Complete with record, store, and tracker implementations.
 */

this.RotaryRecord = function RotaryRecord(collection, id) {
  CryptoWrapper.call(this, collection, id);
}
RotaryRecord.prototype = {
  __proto__: CryptoWrapper.prototype
};
Utils.deferGetSet(RotaryRecord, "cleartext", ["denomination"]);

this.RotaryStore = function RotaryStore(name, engine) {
  Store.call(this, name, engine);
  this.items = {};
}
RotaryStore.prototype = {
  __proto__: Store.prototype,

  create: function create(record) {
    this.items[record.id] = record.denomination;
  },

  remove: function remove(record) {
    delete this.items[record.id];
  },

  update: function update(record) {
    this.items[record.id] = record.denomination;
  },

  itemExists: function itemExists(id) {
    return (id in this.items);
  },

  createRecord: function createRecord(id, collection) {
    let record = new RotaryRecord(collection, id);

    if (!(id in this.items)) {
      record.deleted = true;
      return record;
    }

    record.denomination = this.items[id] || "Data for new record: " + id;
    return record;
  },

  changeItemID: function changeItemID(oldID, newID) {
    if (oldID in this.items) {
      this.items[newID] = this.items[oldID];
    }

    delete this.items[oldID];
  },

  getAllIDs: function getAllIDs() {
    let ids = {};
    for (let id in this.items) {
      ids[id] = true;
    }
    return ids;
  },

  wipe: function wipe() {
    this.items = {};
  }
};

this.RotaryTracker = function RotaryTracker(name, engine) {
  Tracker.call(this, name, engine);
}
RotaryTracker.prototype = {
  __proto__: Tracker.prototype
};


this.RotaryEngine = function RotaryEngine(service) {
  SyncEngine.call(this, "Rotary", service);
  // Ensure that the engine starts with a clean slate.
  this.toFetch        = [];
  this.previousFailed = [];
}
RotaryEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: RotaryStore,
  _trackerObj: RotaryTracker,
  _recordObj: RotaryRecord,

  _findDupe: function _findDupe(item) {
    // This is a semaphore used for testing proper reconciling on dupe
    // detection.
    if (item.id == "DUPE_INCOMING") {
      return "DUPE_LOCAL";
    }

    for (let [id, value] in Iterator(this._store.items)) {
      if (item.denomination == value) {
        return id;
      }
    }
  }
};
