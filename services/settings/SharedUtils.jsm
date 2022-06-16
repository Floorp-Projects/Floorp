/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Common logic shared by RemoteSettingsWorker.js (Worker) and the main thread.
 */

var EXPORTED_SYMBOLS = ["SharedUtils"];

var SharedUtils = {
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

  /**
   * Load (from disk) the JSON file distributed with the release for this collection.
   * @param {String}  bucket
   * @param {String}  collection
   */
  async loadJSONDump(bucket, collection) {
    // When using the preview bucket, we still want to load the main dump.
    // But we store it locally in the preview bucket.
    const jsonBucket = bucket.replace("-preview", "");
    const fileURI = `resource://app/defaults/settings/${jsonBucket}/${collection}.json`;
    let response;
    try {
      response = await fetch(fileURI);
    } catch (e) {
      // Return null if file is missing.
      return { data: null, timestamp: null };
    }
    // Will throw if JSON is invalid.
    return response.json();
  },
};
