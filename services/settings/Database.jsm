/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

// XPCOMUtils.defineLazyGlobalGetters(this, ["indexeddb"]);

XPCOMUtils.defineLazyModuleGetters(this, {
  CommonUtils: "resource://services-common/utils.js",
  Kinto: "resource://services-common/kinto-offline-client.js",
});

var EXPORTED_SYMBOLS = ["Database"];

// IndexedDB name.
const DB_NAME = "remote-settings";

/**
 * Database is a tiny wrapper with the objective
 * of providing major kinto-offline-client collection API.
 * (with the objective of getting rid of kinto-offline-client)
 */
class Database {
  /* Expose the IDBError class publicly */
  static get IDBError() {
    return Kinto.adapters.IDB.IDBError;
  }

  constructor(identifier) {
    this._idb = new Kinto.adapters.IDB(identifier, {
      dbName: DB_NAME,
      migrateOldData: false,
    });
  }

  async list(options) {
    return this._idb.list(options);
  }

  async importBulk(toInsert) {
    return this._idb.importBulk(toInsert);
  }

  async deleteAll(toDelete) {
    return this._idb.execute(transaction => {
      toDelete.forEach(r => {
        transaction.delete(r.id);
      });
    });
  }

  async getLastModified() {
    return this._idb.getLastModified();
  }

  async saveLastModified(remoteTimestamp) {
    return this._idb.saveLastModified(remoteTimestamp);
  }

  async getMetadata() {
    return this._idb.getMetadata();
  }

  async saveMetadata(metadata) {
    return this._idb.saveMetadata(metadata);
  }

  async clear() {
    await this._idb.clear();
    await this._idb.saveLastModified(null);
    await this._idb.saveMetadata(null);
  }

  async close() {
    return this._idb.close();
  }

  /*
   * Methods used by unit tests.
   */

  async create(record) {
    if (!("id" in record)) {
      record = { ...record, id: CommonUtils.generateUUID() };
    }
    return this._idb.execute(transaction => {
      transaction.create(record);
    });
  }

  async update(record) {
    return this._idb.execute(transaction => {
      transaction.update(record);
    });
  }

  async delete(recordId) {
    return this._idb.execute(transaction => {
      transaction.delete(recordId);
    });
  }
}
