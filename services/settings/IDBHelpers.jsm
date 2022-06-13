/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["IDBHelpers"];

const DB_NAME = "remote-settings";
const DB_VERSION = 3;

// `indexedDB` is accessible in the worker global, but not the JSM global,
// where we have to import it - and the worker global doesn't have Cu.
if (typeof indexedDB == "undefined") {
  Cu.importGlobalProperties(["indexedDB"]);
}

/**
 * Wrap IndexedDB errors to catch them more easily.
 */
class IndexedDBError extends Error {
  constructor(error, method = "", identifier = "") {
    if (typeof error == "string") {
      error = new Error(error);
    }
    super(`IndexedDB: ${identifier} ${method} ${error && error.message}`);
    this.name = error.name;
    this.stack = error.stack;
  }
}

class ShutdownError extends IndexedDBError {
  constructor(error, method = "", identifier = "") {
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
function bulkOperationHelper(
  store,
  { reject, completion },
  operation,
  list,
  listIndex = 0
) {
  try {
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
        { reject, completion },
        operation,
        list,
        listIndex
      );
    } else if (completion) {
      completion();
    }
    // otherwise, we're done, and the transaction will complete on its own.
  } catch (e) {
    // The executeIDB callsite has a try... catch, but it will not catch
    // errors in subsequent bulkOperationHelper calls chained through
    // request.onsuccess above. We do want to catch those, so we have to
    // feed them through manually. We cannot use an async function with
    // promises, because if we wait a microtask after onsuccess fires to
    // put more requests on the transaction, the transaction will auto-commit
    // before we can add more requests.
    reject(e);
  }
}

/**
 * Helper to wrap some IDBObjectStore operations into a promise.
 *
 * @param {IDBDatabase} db
 * @param {String|String[]} storeNames - either a string or an array of strings.
 * @param {String} mode
 * @param {function} callback
 * @param {String} description of the operation for error handling purposes.
 */
function executeIDB(db, storeNames, mode, callback, desc) {
  if (!Array.isArray(storeNames)) {
    storeNames = [storeNames];
  }
  const transaction = db.transaction(storeNames, mode);
  let promise = new Promise((resolve, reject) => {
    let stores = storeNames.map(name => transaction.objectStore(name));
    let result;
    let rejectWrapper = e => {
      reject(new IndexedDBError(e, desc || "execute()", storeNames.join(", ")));
      try {
        transaction.abort();
      } catch (ex) {
        Cu.reportError(ex);
      }
    };
    // Add all the handlers before using the stores.
    transaction.onerror = event =>
      reject(new IndexedDBError(event.target.error, desc || "execute()"));
    transaction.onabort = event =>
      reject(
        new IndexedDBError(
          event.target.error || transaction.error || "IDBTransaction aborted",
          desc || "execute()"
        )
      );
    transaction.oncomplete = event => resolve(result);
    // Simplify access to a single datastore:
    if (stores.length == 1) {
      stores = stores[0];
    }
    try {
      // Although this looks sync, once the callback places requests
      // on the datastore, it can independently keep the transaction alive and
      // keep adding requests. Even once we exit this try.. catch, we may
      // therefore experience errors which should abort the transaction.
      // This is why we pass the rejection handler - then the consumer can
      // continue to ensure that errors are handled appropriately.
      // In theory, exceptions thrown from onsuccess handlers should also
      // cause IndexedDB to abort the transaction, so this is a belt-and-braces
      // approach.
      result = callback(stores, rejectWrapper);
    } catch (e) {
      rejectWrapper(e);
    }
  });
  return { promise, transaction };
}

/**
 * Helper to wrap indexedDB.open() into a promise.
 */
async function openIDB(allowUpgrades = true) {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open(DB_NAME, DB_VERSION);
    request.onupgradeneeded = event => {
      if (!allowUpgrades) {
        reject(
          new Error(
            `IndexedDB: Error accessing ${DB_NAME} IDB at version ${DB_VERSION}`
          )
        );
        return;
      }
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
      if (event.oldVersion < 3) {
        // Attachment store
        db.createObjectStore("attachments", {
          keyPath: ["cid", "attachmentId"],
        });
      }
    };
    request.onerror = event => reject(new IndexedDBError(event.target.error));
    request.onsuccess = event => {
      const db = event.target.result;
      resolve(db);
    };
  });
}

function destroyIDB() {
  const request = indexedDB.deleteDatabase(DB_NAME);
  return new Promise((resolve, reject) => {
    request.onerror = event => reject(new IndexedDBError(event.target.error));
    request.onsuccess = () => resolve();
  });
}

var IDBHelpers = {
  bulkOperationHelper,
  executeIDB,
  openIDB,
  destroyIDB,
  IndexedDBError,
  ShutdownError,
};
