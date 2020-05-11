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
  "resource://services-settings/IDBHelpers.jsm",
  "resource://gre/modules/third_party/jsesc/jsesc.js"
);

const IDB_RECORDS_STORE = "records";
const IDB_TIMESTAMPS_STORE = "timestamps";

let gShutdown = false;

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
    if (gShutdown) {
      throw new Error("Can't import when we've started shutting down.");
    }
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
    return this.checkContentHash(buffer, size, hash);
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

  async prepareShutdown() {
    gShutdown = true;
    // Ensure we can iterate and abort (which may delete items) by cloning
    // the list.
    let transactions = Array.from(gPendingTransactions);
    for (let transaction of transactions) {
      try {
        transaction.abort();
      } catch (ex) {
        // We can hit this case if the transaction has finished but
        // we haven't heard about it yet.
      }
    }
  },

  _test_only_import(bucket, collection, records) {
    return importDumpIDB(bucket, collection, records);
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
  if (gShutdown) {
    throw new Error("Can't import when we've started shutting down.");
  }
  // Will throw if JSON is invalid.
  return response.json();
}

let gPendingTransactions = new Set();

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
  const db = await IDBHelpers.openIDB(false /* do not allow upgrades */);

  // try...finally to ensure we always close the db.
  try {
    if (gShutdown) {
      throw new Error("Can't import when we've started shutting down.");
    }

    // Each entry of the dump will be stored in the records store.
    // They are indexed by `_cid`.
    const cid = bucket + "/" + collection;
    // We can just modify the items in-place, as we got them from loadJSONDump.
    records.forEach(item => {
      item._cid = cid;
    });
    // Store the highest timestamp as the collection timestamp (or zero if dump is empty).
    const timestamp =
      records.length === 0
        ? 0
        : Math.max(...records.map(record => record.last_modified));
    let { transaction, promise } = IDBHelpers.executeIDB(
      db,
      [IDB_RECORDS_STORE, IDB_TIMESTAMPS_STORE],
      "readwrite",
      ([recordsStore, timestampStore], rejectTransaction) => {
        IDBHelpers.bulkOperationHelper(
          recordsStore,
          {
            reject: rejectTransaction,
            completion() {
              timestampStore.put({ cid, value: timestamp });
            },
          },
          "put",
          records
        );
      }
    );
    gPendingTransactions.add(transaction);
    promise = promise.finally(() => gPendingTransactions.delete(transaction));
    await promise;
  } finally {
    // Close now that we're done.
    db.close();
  }
}
