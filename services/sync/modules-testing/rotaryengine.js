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

var {utils: Cu} = Components;

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
};
RotaryRecord.prototype = {
  __proto__: CryptoWrapper.prototype
};
Utils.deferGetSet(RotaryRecord, "cleartext", ["denomination"]);

this.RotaryStore = function RotaryStore(name, engine) {
  Store.call(this, name, engine);
  this.items = {};
};
RotaryStore.prototype = {
  __proto__: Store.prototype,

  async create(record) {
    this.items[record.id] = record.denomination;
  },

  async remove(record) {
    delete this.items[record.id];
  },

  async update(record) {
    this.items[record.id] = record.denomination;
  },

  async itemExists(id) {
    return (id in this.items);
  },

  async createRecord(id, collection) {
    let record = new RotaryRecord(collection, id);

    if (!(id in this.items)) {
      record.deleted = true;
      return record;
    }

    record.denomination = this.items[id] || "Data for new record: " + id;
    return record;
  },

  async changeItemID(oldID, newID) {
    if (oldID in this.items) {
      this.items[newID] = this.items[oldID];
    }

    delete this.items[oldID];
  },

  async getAllIDs() {
    let ids = {};
    for (let id in this.items) {
      ids[id] = true;
    }
    return ids;
  },

  async wipe() {
    this.items = {};
  }
};

this.RotaryTracker = function RotaryTracker(name, engine) {
  Tracker.call(this, name, engine);
};
RotaryTracker.prototype = {
  __proto__: Tracker.prototype,
  persistChangedIDs: false,
};


this.RotaryEngine = function RotaryEngine(service) {
  SyncEngine.call(this, "Rotary", service);
  // Ensure that the engine starts with a clean slate.
  this.toFetch        = [];
  this.previousFailed = [];
};
RotaryEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _storeObj: RotaryStore,
  _trackerObj: RotaryTracker,
  _recordObj: RotaryRecord,

  async _findDupe(item) {
    // This is a Special Value® used for testing proper reconciling on dupe
    // detection.
    if (item.id == "DUPE_INCOMING") {
      return "DUPE_LOCAL";
    }

    for (let [id, value] of Object.entries(this._store.items)) {
      if (item.denomination == value) {
        return id;
      }
    }
    return null;
  }
};
