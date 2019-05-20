/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Interface to a dedicated thread handling for Remote Settings heavy operations.
 */
const { XPCOMUtils } = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const { setTimeout, clearTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

var EXPORTED_SYMBOLS = ["RemoteSettingsWorker"];

XPCOMUtils.defineLazyPreferenceGetter(this, "gMaxIdleMilliseconds",
  "services.settings.worker_idle_max_milliseconds",
  30 * 1000 // Default of 30 seconds.
);

class Worker {
  constructor(source) {
    this.source = source;
    this.worker = null;

    this.callbacks = new Map();
    this.lastCallbackId = 0;
    this.idleTimeoutId = null;
  }

  async _execute(method, args = []) {
    // (Re)instantiate the worker if it was terminated.
    if (!this.worker) {
      this.worker = new ChromeWorker(this.source);
      this.worker.onmessage = this._onWorkerMessage.bind(this);
    }
    // New activity: reset the idle timer.
    if (this.idleTimeoutId) {
      clearTimeout(this.idleTimeoutId);
    }
    return new Promise((resolve, reject) => {
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

    // Terminate the worker when it's unused for some time.
    // But don't terminate it if an operation is pending.
    if (this.callbacks.length == 0) {
      this.idleTimeoutId = setTimeout(() => {
        this.worker.terminate();
        this.worker = null;
        this.idleTimeoutId = null;
      }, gMaxIdleMilliseconds);
    }
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
