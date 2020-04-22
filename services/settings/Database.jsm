/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["indexedDB"]);

XPCOMUtils.defineLazyModuleGetters(this, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  CommonUtils: "resource://services-common/utils.js",
});

var EXPORTED_SYMBOLS = ["Database"];

// IndexedDB name.
const DB_NAME = "remote-settings";
const DB_VERSION = 2;

/**
 * Wrap IndexedDB errors to catch them more easily.
 */
class IndexedDBError extends Error {
  constructor(error, method = "", identifier = "") {
    super(`IndexedDB: ${identifier} ${method} ${error.message}`);
    this.name = error.name;
    this.stack = error.stack;
  }
}

class ShutdownError extends IndexedDBError {
  constructor(error, method = "", identifier = "") {
    if (typeof error == "string") {
      error = new Error(error);
    }
    super(error, method, identifier);
  }
}

// We batch operations in order to reduce round-trip latency to the IndexedDB
// database thread. The trade-offs are that the more records in the batch, the
// more time we spend on this thread in structured serialization, and the
// greater the chance to jank PBackground and this thread when the responses
// come back. The initial choice of 250 was made targeting 2-3ms on a fast
// machine and 10-15ms on a slow machine.
// Every chunk waits for success before starting the next, and
// the final chunk's completion will fire transaction.oncomplete .
function bulkOperationHelper(store, operation, list, listIndex = 0) {
  const CHUNK_LENGTH = 250;
  const max = Math.min(listIndex + CHUNK_LENGTH, list.length);
  let request;
  for (; listIndex < max; listIndex++) {
    request = store[operation](list[listIndex]);
  }
  if (listIndex < list.length) {
    // On error, `transaction.onerror` is called.
    request.onsuccess = bulkOperationHelper.bind(
      null,
      store,
      operation,
      list,
      listIndex
    );
  }
  // otherwise, we're done, and the transaction will complete on its own.
}

/**
 * Database is a tiny wrapper with the objective
 * of providing major kinto-offline-client collection API.
 * (with the objective of getting rid of kinto-offline-client)
 */
class Database {
  /* Expose the IDBError and ShutdownError class publicly */
  static get IDBError() {
    return IndexedDBError;
  }

  static get ShutdownError() {
    return ShutdownError;
  }

  constructor(identifier) {
    ensureShutdownBlocker();
    this.identifier = identifier;
  }

  async list(options = {}) {
    const { filters = {}, sort = "" } = options;
    const objFilters = transformSubObjectFilters(filters);
    let results = [];
    try {
      await executeIDB(
        "records",
        store => {
          const request = store
            .index("cid")
            .openCursor(IDBKeyRange.only(this.identifier));
          request.onsuccess = event => {
            const cursor = event.target.result;
            if (cursor) {
              const { value } = cursor;
              if (filterObject(objFilters, value)) {
                results.push(value);
              }
              cursor.continue();
            }
          };
        },
        { mode: "readonly" }
      );
    } catch (e) {
      throw new IndexedDBError(e, "list()", this.identifier);
    }
    // Remove IDB key field from results.
    for (const result of results) {
      delete result._cid;
    }
    return sort ? sortObjects(sort, results) : results;
  }

  async importBulk(toInsert) {
    const _cid = this.identifier;
    try {
      await executeIDB(
        "records",
        store => {
          bulkOperationHelper(
            store,
            "put",
            toInsert.map(item => {
              return Object.assign({ _cid }, item);
            })
          );
        },
        { desc: "importBulk() in " + this.identifier }
      );
    } catch (e) {
      throw new IndexedDBError(e, "importBulk()", this.identifier);
    }
  }

  async deleteBulk(toDelete) {
    const _cid = this.identifier;
    try {
      await executeIDB(
        "records",
        store => {
          bulkOperationHelper(
            store,
            "delete",
            toDelete.map(item => {
              return [_cid, item.id];
            })
          );
        },
        { desc: "deleteBulk() in " + this.identifier }
      );
    } catch (e) {
      throw new IndexedDBError(e, "deleteBulk()", this.identifier);
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
      throw new IndexedDBError(e, "getLastModified()", this.identifier);
    }
    return entry ? entry.value : null;
  }

  async saveLastModified(lastModified) {
    const value = parseInt(lastModified, 10) || null;
    try {
      await executeIDB(
        "timestamps",
        store => {
          if (value === null) {
            store.delete(this.identifier);
          } else {
            store.put({ cid: this.identifier, value });
          }
        },
        { desc: "saveLastModified() in " + this.identifier }
      );
    } catch (e) {
      throw new IndexedDBError(e, "saveLastModified()", this.identifier);
    }
    return value;
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
      throw new IndexedDBError(e, "getMetadata()", this.identifier);
    }
    return entry ? entry.metadata : null;
  }

  async saveMetadata(metadata) {
    try {
      await executeIDB(
        "collections",
        store => store.put({ cid: this.identifier, metadata }),
        { desc: "saveMetadata() in " + this.identifier }
      );
      return metadata;
    } catch (e) {
      throw new IndexedDBError(e, "saveMetadata()", this.identifier);
    }
  }

  async clear() {
    try {
      await this.saveLastModified(null);
      await this.saveMetadata(null);
      await executeIDB(
        "records",
        store => {
          const range = IDBKeyRange.only(this.identifier);
          const request = store.index("cid").openKeyCursor(range);
          request.onsuccess = event => {
            const cursor = event.target.result;
            if (cursor) {
              store.delete(cursor.primaryKey);
              cursor.continue();
            }
          };
          return request;
        },
        { desc: "clear() in " + this.identifier }
      );
    } catch (e) {
      throw new IndexedDBError(e, "clear()", this.identifier);
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
      throw new IndexedDBError(e, "create()", this.identifier);
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
      throw new IndexedDBError(e, "update()", this.identifier);
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
      throw new IndexedDBError(e, "delete()", this.identifier);
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
async function openIDB(callback) {
  // We can be called multiple times in a race; always ensure that when
  // we complete, `gDB` is no longer null, but avoid doing the actual
  // IndexedDB work more than once.
  if (!gDBPromise) {
    // Open and initialize/upgrade if needed.
    gDBPromise = new Promise((resolve, reject) => {
      const request = indexedDB.open(DB_NAME, DB_VERSION);
      request.onupgradeneeded = event => {
        // When an upgrade is needed, a transaction is started.
        const transaction = event.target.transaction;
        transaction.onabort = event => {
          const error =
            event.target.error ||
            transaction.error ||
            new DOMException("The operation has been aborted", "AbortError");
          reject(new IndexedDBError(error, "open()"));
        };

        const db = event.target.result;
        db.onerror = event => reject(new IndexedDBError(event.target.error));

        if (event.oldVersion < 1) {
          // Records store
          const recordsStore = db.createObjectStore("records", {
            keyPath: ["_cid", "id"],
          });
          // An index to obtain all the records in a collection.
          recordsStore.createIndex("cid", "_cid");
          // Last modified field
          recordsStore.createIndex("last_modified", ["_cid", "last_modified"]);
          // Timestamps store
          db.createObjectStore("timestamps", {
            keyPath: "cid",
          });
        }
        if (event.oldVersion < 2) {
          // Collections store
          db.createObjectStore("collections", {
            keyPath: "cid",
          });
        }
      };
      request.onerror = event =>
        reject(new IndexedDBError(event.target.error, "open()"));
      request.onsuccess = event => {
        const db = event.target.result;
        resolve(db);
      };
    });
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
 * @param {String} storeName
 * @param {function} callback
 * @param {Object} options
 * @param {String} options.mode
 * @param {String} options.desc   for shutdown tracking.
 */
async function executeIDB(storeName, callback, options = {}) {
  if (!gDB) {
    // Check if we're shutting down. Services.startup.shuttingDown will
    // be true sooner, but is never true in xpcshell tests, so we check
    // both that and a bool we set ourselves when `profile-before-change`
    // starts.
    if (gShutdownStarted || Services.startup.shuttingDown) {
      throw new ShutdownError("The application is shutting down", "execute()");
    }
    await openIDB();
  } else {
    // Even if we have a db, wait a tick to avoid making IndexedDB sad.
    // We should be able to remove this once bug 1626935 is fixed.
    await Promise.resolve();
  }
  let db = gDB;
  const { mode = "readwrite" } = options;
  const transaction = db.transaction([storeName], mode);

  let promise = new Promise((resolve, reject) => {
    const store = transaction.objectStore(storeName);
    let result;
    try {
      result = callback(store);
    } catch (e) {
      transaction.abort();
      reject(new IndexedDBError(e, "execute()", storeName));
    }
    transaction.onerror = event =>
      reject(new IndexedDBError(event.target.error, "execute()"));
    transaction.oncomplete = event => resolve(result);
  });
  // We track all readonly transactions and abort them at shutdown.
  // We track all the other ones and await their completion at
  // shutdown (to avoid dataloss when writes fail).
  if (mode == "readonly") {
    gPendingReadOnlyTransactions.add(transaction);
    promise
      .finally(() => gPendingReadOnlyTransactions.delete(transaction))
      // The finally clause creates another promise, which will also
      // be in a "rejected" state if `promise` is rejected. To not
      // upset our "uncaught promise rejection" handler, no-op catch
      // rejections on the end of that chain:
      .catch(() => {});
  } else {
    let obj = { promise, desc: options.desc };
    gPendingWriteOperations.add(obj);
    promise
      .finally(() => gPendingWriteOperations.delete(obj))
      // Same as above.
      .catch(() => {});
  }
  return promise;
}

function _isUndefined(value) {
  return typeof value === "undefined";
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

/**
 * Test if a single object matches all given filters.
 *
 * @param  {Object} filters  The filters object.
 * @param  {Object} entry    The object to filter.
 * @return {Boolean}
 */
function filterObject(filters, entry) {
  return Object.entries(filters).every(([filter, value]) => {
    if (Array.isArray(value)) {
      return value.some(candidate => candidate === entry[filter]);
    } else if (typeof value === "object") {
      return filterObject(value, entry[filter]);
    } else if (!Object.prototype.hasOwnProperty.call(entry, filter)) {
      console.error(`The property ${filter} does not exist`);
      return false;
    }
    return entry[filter] === value;
  });
}

/**
 * Sorts records in a list according to a given ordering.
 *
 * @param  {String} order The ordering, eg. `-last_modified`.
 * @param  {Array}  list  The collection to order.
 * @return {Array}
 */
function sortObjects(order, list) {
  const hasDash = order[0] === "-";
  const field = hasDash ? order.slice(1) : order;
  const direction = hasDash ? -1 : 1;
  return list.slice().sort((a, b) => {
    if (a[field] && _isUndefined(b[field])) {
      return direction;
    }
    if (b[field] && _isUndefined(a[field])) {
      return -direction;
    }
    if (_isUndefined(a[field]) && _isUndefined(b[field])) {
      return 0;
    }
    return a[field] > b[field] ? direction : -direction;
  });
}

let gShutdownBlocker = false;
let gShutdownStarted = false;
function ensureShutdownBlocker() {
  if (gShutdownBlocker) {
    return;
  }
  gShutdownBlocker = true;
  AsyncShutdown.profileBeforeChange.addBlocker(
    "RemoteSettingsClient - finish IDB access.",
    () => {
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
    },
    {
      fetchState() {
        return Array.from(gPendingWriteOperations).map(op => op.desc);
      },
    }
  );
}
