/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/chrome-worker */

"use strict";

/**
 * A worker dedicated to Remote Settings.
 */

importScripts(
  "resource://gre/modules/workers/require.js",
  "resource://gre/modules/CanonicalJSON.jsm",
  "resource://gre/modules/third_party/jsesc/jsesc.js"
);

const IDB_NAME = "remote-settings";
const IDB_VERSION = 2;
const IDB_RECORDS_STORE = "records";
const IDB_TIMESTAMPS_STORE = "timestamps";

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

const Agent = {
  /**
   * Return the canonical JSON serialization of the specified records.
   * It has to match what is done on the server (See Kinto/kinto-signer).
   *
   * @param {Array<Object>} records
   * @param {String} timestamp
   * @returns {String}
   */
  async canonicalStringify(records, timestamp) {
    // Sort list by record id.
    let allRecords = records.sort((a, b) => {
      if (a.id < b.id) {
        return -1;
      }
      return a.id > b.id ? 1 : 0;
    });
    // All existing records are replaced by the version from the server
    // and deleted records are removed.
    for (let i = 0; i < allRecords.length /* no increment! */; ) {
      const rec = allRecords[i];
      const next = allRecords[i + 1];
      if ((next && rec.id == next.id) || rec.deleted) {
        allRecords.splice(i, 1); // remove local record
      } else {
        i++;
      }
    }
    const toSerialize = {
      last_modified: "" + timestamp,
      data: allRecords,
    };
    return CanonicalJSON.stringify(toSerialize, jsesc);
  },

  /**
   * If present, import the JSON file into the Remote Settings IndexedDB
   * for the specified bucket and collection.
   * (eg. blocklists/certificates, main/onboarding)
   * @param {String} bucket
   * @param {String} collection
   */
  async importJSONDump(bucket, collection) {
    const { data: records } = await loadJSONDump(bucket, collection);
    await importDumpIDB(bucket, collection, records);
    return records.length;
  },

  /**
   * Check that the specified file matches the expected size and SHA-256 hash.
   * @param {String} fileUrl file URL to read from
   * @param {Number} size expected file size
   * @param {String} size expected file SHA-256 as hex string
   * @returns {boolean}
   */
  async checkFileHash(fileUrl, size, hash) {
    let resp;
    try {
      resp = await fetch(fileUrl);
    } catch (e) {
      // File does not exist.
      return false;
    }
    const buffer = await resp.arrayBuffer();
    const bytes = new Uint8Array(buffer);
    return this.checkContentHash(bytes, size, hash);
  },

  /**
   * Check that the specified content matches the expected size and SHA-256 hash.
   * @param {ArrayBuffer} buffer binary content
   * @param {Number} size expected file size
   * @param {String} size expected file SHA-256 as hex string
   * @returns {boolean}
   */
  async checkContentHash(buffer, size, hash) {
    const bytes = new Uint8Array(buffer);
    // Has expected size? (saves computing hash)
    if (bytes.length !== size) {
      return false;
    }
    // Has expected content?
    const hashBuffer = await crypto.subtle.digest("SHA-256", bytes);
    const hashBytes = new Uint8Array(hashBuffer);
    const toHex = b => b.toString(16).padStart(2, "0");
    const hashStr = Array.from(hashBytes, toHex).join("");
    return hashStr == hash;
  },
};

/**
 * Wrap worker invocations in order to return the `callbackId` along
 * the result. This will allow to transform the worker invocations
 * into promises in `RemoteSettingsWorker.jsm`.
 */
self.onmessage = event => {
  const { callbackId, method, args = [] } = event.data;
  Agent[method](...args)
    .then(result => {
      self.postMessage({ callbackId, result });
    })
    .catch(error => {
      console.log(`RemoteSettingsWorker error: ${error}`);
      self.postMessage({ callbackId, error: "" + error });
    });
};

/**
 * Load (from disk) the JSON file distributed with the release for this collection.
 * @param {String}  bucket
 * @param {String}  collection
 */
async function loadJSONDump(bucket, collection) {
  const fileURI = `resource://app/defaults/settings/${bucket}/${collection}.json`;
  let response;
  try {
    response = await fetch(fileURI);
  } catch (e) {
    // Return empty dataset if file is missing.
    return { data: [] };
  }
  // Will throw if JSON is invalid.
  return response.json();
}

/**
 * Import the records into the Remote Settings Chrome IndexedDB.
 *
 * Note: This duplicates some logics from `kinto-offline-client.js`.
 *
 * @param {String} bucket
 * @param {String} collection
 * @param {Array<Object>} records
 */
async function importDumpIDB(bucket, collection, records) {
  // Open the DB. It will exist since if we are running this, it means
  // we already tried to read the timestamp in `remote-settings.js`
  const db = await openIDB(IDB_NAME, IDB_VERSION);

  // Each entry of the dump will be stored in the records store.
  // They are indexed by `_cid`.
  const cid = bucket + "/" + collection;
  await executeIDB(db, IDB_RECORDS_STORE, store => {
    // We can just modify the items in-place, as we got them from loadJSONDump.
    records.forEach(item => {
      item._cid = cid;
    });
    bulkOperationHelper(store, "put", records);
  });

  // Store the highest timestamp as the collection timestamp (or zero if dump is empty).
  const timestamp =
    records.length === 0
      ? 0
      : Math.max(...records.map(record => record.last_modified));
  await executeIDB(db, IDB_TIMESTAMPS_STORE, store =>
    store.put({ cid, value: timestamp })
  );
  // Close now that we're done.
  db.close();
}

/**
 * Wrap IndexedDB errors to catch them more easily.
 */
class IndexedDBError extends Error {
  constructor(error) {
    super(`IndexedDB: ${error.message}`);
    this.name = error.name;
    this.stack = error.stack;
  }
}

/**
 * Helper to wrap indexedDB.open() into a promise.
 */
async function openIDB(dbname, version) {
  return new Promise((resolve, reject) => {
    const request = indexedDB.open(dbname, version);
    request.onupgradeneeded = () => {
      // We should never have to initialize the DB here.
      reject(
        new Error(
          `IndexedDB: Error accessing ${dbname} Chrome IDB at version ${version}`
        )
      );
    };
    request.onerror = event => reject(new IndexedDBError(event.target.error));
    request.onsuccess = event => {
      const db = event.target.result;
      resolve(db);
    };
  });
}

/**
 * Helper to wrap some IDBObjectStore operations into a promise.
 *
 * @param {IDBDatabase} db
 * @param {String} storeName
 * @param {function} callback
 */
async function executeIDB(db, storeName, callback) {
  const mode = "readwrite";
  return new Promise((resolve, reject) => {
    const transaction = db.transaction([storeName], mode);
    const store = transaction.objectStore(storeName);
    let result;
    try {
      result = callback(store);
    } catch (e) {
      transaction.abort();
      reject(new IndexedDBError(e));
    }
    transaction.onerror = event =>
      reject(new IndexedDBError(event.target.error));
    transaction.oncomplete = event => resolve(result);
  });
}
