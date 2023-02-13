/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  Store,
  SyncEngine,
  LegacyTracker,
} from "resource://services-sync/engines.sys.mjs";

import { CryptoWrapper } from "resource://services-sync/record.sys.mjs";
import { SerializableSet, Utils } from "resource://services-sync/util.sys.mjs";

/*
 * A fake engine implementation.
 * This is used all over the place.
 *
 * Complete with record, store, and tracker implementations.
 */

export function RotaryRecord(collection, id) {
  CryptoWrapper.call(this, collection, id);
}

RotaryRecord.prototype = {};
Object.setPrototypeOf(RotaryRecord.prototype, CryptoWrapper.prototype);
Utils.deferGetSet(RotaryRecord, "cleartext", ["denomination"]);

export function RotaryStore(name, engine) {
  Store.call(this, name, engine);
  this.items = {};
}

RotaryStore.prototype = {
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
    return id in this.items;
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
  },
};

Object.setPrototypeOf(RotaryStore.prototype, Store.prototype);

export function RotaryTracker(name, engine) {
  LegacyTracker.call(this, name, engine);
}

RotaryTracker.prototype = {};
Object.setPrototypeOf(RotaryTracker.prototype, LegacyTracker.prototype);

export function RotaryEngine(service) {
  SyncEngine.call(this, "Rotary", service);
  // Ensure that the engine starts with a clean slate.
  this.toFetch = new SerializableSet();
  this.previousFailed = new SerializableSet();
}

RotaryEngine.prototype = {
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
  },
};
Object.setPrototypeOf(RotaryEngine.prototype, SyncEngine.prototype);
