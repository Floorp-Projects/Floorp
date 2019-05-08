/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Interface to a dedicated thread handling for Remote Settings heavy operations.
 */

// ChromeUtils.import("resource://gre/modules/PromiseWorker.jsm", this);

var EXPORTED_SYMBOLS = ["RemoteSettingsWorker"];

class Worker {
  constructor(source) {
    this.worker = new ChromeWorker(source);
    this.worker.onmessage = this._onWorkerMessage.bind(this);

    this.callbacks = new Map();
    this.lastCallbackId = 0;
  }

  async _execute(method, args = []) {
    return new Promise(async (resolve, reject) => {
      const callbackId = ++this.lastCallbackId;
      this.callbacks.set(callbackId, [resolve, reject]);
      this.worker.postMessage({ callbackId, method, args });
    });
  }

  _onWorkerMessage(event) {
    const { callbackId, result, error } = event.data;
    const [resolve, reject] = this.callbacks.get(callbackId);
    if (error) {
      reject(new Error(error));
    } else {
      resolve(result);
    }
    this.callbacks.delete(callbackId);
  }

  async canonicalStringify(localRecords, remoteRecords, timestamp) {
    return this._execute("canonicalStringify", [localRecords, remoteRecords, timestamp]);
  }

  async importJSONDump(bucket, collection) {
    return this._execute("importJSONDump", [bucket, collection]);
  }

  async checkFileHash(filepath, size, hash) {
    return this._execute("checkFileHash", [filepath, size, hash]);
  }
}

var RemoteSettingsWorker = new Worker("resource://services-settings/RemoteSettingsWorker.js");
