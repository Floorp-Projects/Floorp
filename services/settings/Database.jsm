/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  IDBHelpers: "resource://services-settings/IDBHelpers.jsm",
  Utils: "resource://services-settings/Utils.jsm",
  CommonUtils: "resource://services-common/utils.js",
  ObjectUtils: "resource://gre/modules/ObjectUtils.jsm",
});
XPCOMUtils.defineLazyGetter(this, "console", () => Utils.log);

var EXPORTED_SYMBOLS = ["Database"];

/**
 * Database is a tiny wrapper with the objective
 * of providing major kinto-offline-client collection API.
 * (with the objective of getting rid of kinto-offline-client)
 */
class Database {
  constructor(identifier) {
    ensureShutdownBlocker();
    this.identifier = identifier;
  }

  async list(options = {}) {
    const { filters = {}, order = "" } = options;
    let results = [];
    try {
      await executeIDB(
        "records",
        (store, rejectTransaction) => {
          // Fast-path the (very common) no-filters case
          if (ObjectUtils.isEmpty(filters)) {
            const range = IDBKeyRange.only(this.identifier);
            const request = store.index("cid").getAll(range);
            request.onsuccess = e => {
              results = e.target.result;
            };
            return;
          }
          const request = store
            .index("cid")
            .openCursor(IDBKeyRange.only(this.identifier));
          const objFilters = transformSubObjectFilters(filters);
          request.onsuccess = event => {
            try {
              const cursor = event.target.result;
              if (cursor) {
                const { value } = cursor;
                if (Utils.filterObject(objFilters, value)) {
                  results.push(value);
                }
                cursor.continue();
              }
            } catch (ex) {
              rejectTransaction(ex);
            }
          };
        },
        { mode: "readonly" }
      );
    } catch (e) {
      throw new IDBHelpers.IndexedDBError(e, "list()", this.identifier);
    }
    // Remove IDB key field from results.
    for (const result of results) {
      delete result._cid;
    }
    return order ? Utils.sortObjects(order, results) : results;
  }

  async importChanges(metadata, timestamp, records = [], options = {}) {
    const { clear = false } = options;
    const _cid = this.identifier;
    try {
      await executeIDB(
        ["collections", "timestamps", "records"],
        (stores, rejectTransaction) => {
          const [storeMetadata, storeTimestamps, storeRecords] = stores;

          if (clear) {
            // Our index is over the _cid and id fields. We want to remove
            // all of the items in the collection for which the object was
            // created, ie with _cid == this.identifier.
            // We would like to just tell IndexedDB:
            // store.index(IDBKeyRange.only(this.identifier)).delete();
            // to delete all records matching the first part of the 2-part key.
            // Unfortunately such an API does not exist.
            // While we could iterate over the index with a cursor, we'd do
            // a roundtrip to PBackground for each item. Once you have 1000
            // items, the result is very slow because of all the overhead of
            // jumping between threads and serializing/deserializing.
            // So instead, we tell the store to delete everything between
            // "our" _cid identifier, and what would be the next identifier
            // (via lexicographical sorting). Unfortunately there does not
            // seem to be a way to specify bounds for all items that share
            // the same first part of the key using just that first part, hence
            // the use of the hypothetical [] for the second part of the end of
            // the bounds.
            storeRecords.delete(
              IDBKeyRange.bound([_cid], [_cid, []], false, true)
            );
          }

          // Store or erase metadata.
          if (metadata === null) {
            storeMetadata.delete(_cid);
          } else if (metadata) {
            storeMetadata.put({ cid: _cid, metadata });
          }
          // Store or erase timestamp.
          if (timestamp === null) {
            storeTimestamps.delete(_cid);
          } else if (timestamp) {
            storeTimestamps.put({ cid: _cid, value: timestamp });
          }

          if (records.length == 0) {
            return;
          }

          // Separate tombstones from creations/updates.
          const toDelete = records.filter(r => r.deleted);
          const toInsert = records.filter(r => !r.deleted);
          console.debug(
            `${_cid} ${toDelete.length} to delete, ${toInsert.length} to insert`
          );
          // Delete local records for each tombstone.
          IDBHelpers.bulkOperationHelper(
            storeRecords,
            {
              reject: rejectTransaction,
              completion() {
                // Overwrite all other data.
                IDBHelpers.bulkOperationHelper(
                  storeRecords,
                  {
                    reject: rejectTransaction,
                  },
                  "put",
                  toInsert.map(item => ({ ...item, _cid }))
                );
              },
            },
            "delete",
            toDelete.map(item => [_cid, item.id])
          );
        },
        { desc: "importChanges() in " + _cid }
      );
    } catch (e) {
      throw new IDBHelpers.IndexedDBError(e, "importChanges()", _cid);
    }
  }

  async getLastModified() {
    let entry = null;
    try {
      await executeIDB(
        "timestamps",
        store => {
          store.get(this.identifier).onsuccess = e => (entry = e.target.result);
        },
        { mode: "readonly" }
      );
    } catch (e) {
      throw new IDBHelpers.IndexedDBError(
        e,
        "getLastModified()",
        this.identifier
      );
    }
    return entry ? entry.value : null;
  }

  async getMetadata() {
    let entry = null;
    try {
      await executeIDB(
        "collections",
        store => {
          store.get(this.identifier).onsuccess = e => (entry = e.target.result);
        },
        { mode: "readonly" }
      );
    } catch (e) {
      throw new IDBHelpers.IndexedDBError(e, "getMetadata()", this.identifier);
    }
    return entry ? entry.metadata : null;
  }

  async getAttachment(attachmentId) {
    let entry = null;
    try {
      await executeIDB(
        "attachments",
        store => {
          store.get([this.identifier, attachmentId]).onsuccess = e => {
            entry = e.target.result;
          };
        },
        { mode: "readonly" }
      );
    } catch (e) {
      throw new IDBHelpers.IndexedDBError(
        e,
        "getAttachment()",
        this.identifier
      );
    }
    return entry ? entry.attachment : null;
  }

  async saveAttachment(attachmentId, attachment) {
    try {
      await executeIDB(
        "attachments",
        store => {
          if (attachment) {
            store.put({ cid: this.identifier, attachmentId, attachment });
          } else {
            store.delete([this.identifier, attachmentId]);
          }
        },
        { desc: "saveAttachment(" + attachmentId + ") in " + this.identifier }
      );
    } catch (e) {
      throw new IDBHelpers.IndexedDBError(
        e,
        "saveAttachment()",
        this.identifier
      );
    }
  }

  async clear() {
    try {
      await this.importChanges(null, null, [], { clear: true });
    } catch (e) {
      throw new IDBHelpers.IndexedDBError(e, "clear()", this.identifier);
    }
  }

  /*
   * Methods used by unit tests.
   */

  async create(record) {
    if (!("id" in record)) {
      record = { ...record, id: CommonUtils.generateUUID() };
    }
    try {
      await executeIDB(
        "records",
        store => {
          store.add({ ...record, _cid: this.identifier });
        },
        { desc: "create() in " + this.identifier }
      );
    } catch (e) {
      throw new IDBHelpers.IndexedDBError(e, "create()", this.identifier);
    }
    return record;
  }

  async update(record) {
    try {
      await executeIDB(
        "records",
        store => {
          store.put({ ...record, _cid: this.identifier });
        },
        { desc: "update() in " + this.identifier }
      );
    } catch (e) {
      throw new IDBHelpers.IndexedDBError(e, "update()", this.identifier);
    }
  }

  async delete(recordId) {
    try {
      await executeIDB(
        "records",
        store => {
          store.delete([this.identifier, recordId]); // [_cid, id]
        },
        { desc: "delete() in " + this.identifier }
      );
    } catch (e) {
      throw new IDBHelpers.IndexedDBError(e, "delete()", this.identifier);
    }
  }
}

let gDB = null;
let gDBPromise = null;

/**
 * This function attempts to ensure `gDB` points to a valid database value.
 * If gDB is already a database, it will do no-op (but this may take a
 * microtask or two).
 * If opening the database fails, it will throw an IndexedDBError.
 */
async function openIDB() {
  // We can be called multiple times in a race; always ensure that when
  // we complete, `gDB` is no longer null, but avoid doing the actual
  // IndexedDB work more than once.
  if (!gDBPromise) {
    // Open and initialize/upgrade if needed.
    gDBPromise = IDBHelpers.openIDB();
  }
  let db = await gDBPromise;
  if (!gDB) {
    gDB = db;
  }
}

const gPendingReadOnlyTransactions = new Set();
const gPendingWriteOperations = new Set();
/**
 * Helper to wrap some IDBObjectStore operations into a promise.
 *
 * @param {IDBDatabase} db
 * @param {String|String[]} storeNames - either a string or an array of strings.
 * @param {function} callback
 * @param {Object} options
 * @param {String} options.mode
 * @param {String} options.desc   for shutdown tracking.
 */
async function executeIDB(storeNames, callback, options = {}) {
  if (!gDB) {
    // Check if we're shutting down. Services.startup.shuttingDown will
    // be true sooner, but is never true in xpcshell tests, so we check
    // both that and a bool we set ourselves when `profile-before-change`
    // starts.
    if (gShutdownStarted || Services.startup.shuttingDown) {
      throw new IDBHelpers.ShutdownError(
        "The application is shutting down",
        "execute()"
      );
    }
    await openIDB();
  } else {
    // Even if we have a db, wait a tick to avoid making IndexedDB sad.
    // We should be able to remove this once bug 1626935 is fixed.
    await Promise.resolve();
  }

  // Check for shutdown again as we've await'd something...
  if (!gDB && (gShutdownStarted || Services.startup.shuttingDown)) {
    throw new IDBHelpers.ShutdownError(
      "The application is shutting down",
      "execute()"
    );
  }

  // Start the actual transaction:
  const { mode = "readwrite", desc = "" } = options;
  let { promise, transaction } = IDBHelpers.executeIDB(
    gDB,
    storeNames,
    mode,
    callback,
    desc
  );

  // We track all readonly transactions and abort them at shutdown.
  // We track all readwrite ones and await their completion at shutdown
  // (to avoid dataloss when writes fail).
  // We use a `.finally()` clause for this; it'll run the function irrespective
  // of whether the promise resolves or rejects, and the promise it returns
  // will resolve/reject with the same value.
  let finishedFn;
  if (mode == "readonly") {
    gPendingReadOnlyTransactions.add(transaction);
    finishedFn = () => gPendingReadOnlyTransactions.delete(transaction);
  } else {
    let obj = { promise, desc };
    gPendingWriteOperations.add(obj);
    finishedFn = () => gPendingWriteOperations.delete(obj);
  }
  return promise.finally(finishedFn);
}

function makeNestedObjectFromArr(arr, val, nestedFiltersObj) {
  const last = arr.length - 1;
  return arr.reduce((acc, cv, i) => {
    if (i === last) {
      return (acc[cv] = val);
    } else if (Object.prototype.hasOwnProperty.call(acc, cv)) {
      return acc[cv];
    }
    return (acc[cv] = {});
  }, nestedFiltersObj);
}

function transformSubObjectFilters(filtersObj) {
  const transformedFilters = {};
  for (const [key, val] of Object.entries(filtersObj)) {
    const keysArr = key.split(".");
    makeNestedObjectFromArr(keysArr, val, transformedFilters);
  }
  return transformedFilters;
}

// We need to expose this wrapper function so we can test
// shutdown handling.
Database._executeIDB = executeIDB;

let gShutdownStarted = false;
// Test-only helper to be able to test shutdown multiple times:
Database._cancelShutdown = () => {
  gShutdownStarted = false;
};

let gShutdownBlocker = false;
Database._shutdownHandler = () => {
  gShutdownStarted = true;
  const NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR = 0x80660006;
  // Duplicate the list (to avoid it being modified) and then
  // abort all read-only transactions.
  for (let transaction of Array.from(gPendingReadOnlyTransactions)) {
    try {
      transaction.abort();
    } catch (ex) {
      // Ensure we don't throw/break, because either way we're in shutdown.

      // In particular, `transaction.abort` can throw if the transaction
      // is complete, ie if we manage to get called inbetween the
      // transaction completing, and our completion handler being called
      // to remove the item from the set. We don't care about that.
      if (ex.result != NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR) {
        // Report any other errors:
        Cu.reportError(ex);
      }
    }
  }
  if (gDB) {
    // This will return immediately; the actual close will happen once
    // there are no more running transactions.
    gDB.close();
    gDB = null;
  }
  gDBPromise = null;
  return Promise.allSettled(
    Array.from(gPendingWriteOperations).map(op => op.promise)
  );
};

function ensureShutdownBlocker() {
  if (gShutdownBlocker) {
    return;
  }
  gShutdownBlocker = true;
  AsyncShutdown.profileBeforeChange.addBlocker(
    "RemoteSettingsClient - finish IDB access.",
    Database._shutdownHandler,
    {
      fetchState() {
        return Array.from(gPendingWriteOperations).map(op => op.desc);
      },
    }
  );
}
