/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Interface to a dedicated thread handling for Remote Settings heavy operations.
 */
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { setTimeout, clearTimeout } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm"
);

var EXPORTED_SYMBOLS = ["RemoteSettingsWorker"];

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gMaxIdleMilliseconds",
  "services.settings.worker_idle_max_milliseconds",
  30 * 1000 // Default of 30 seconds.
);

ChromeUtils.defineModuleGetter(
  this,
  "AsyncShutdown",
  "resource://gre/modules/AsyncShutdown.jsm"
);

// Note: we currently only ever construct one instance of Worker.
// If it stops being a singleton, the AsyncShutdown code at the bottom
// of this file, as well as these globals, will need adjusting.
let gShutdown = false;
let gShutdownResolver = null;

class Worker {
  constructor(source) {
    if (gShutdown) {
      Cu.reportError("Can't create worker once shutdown has started");
    }
    this.source = source;
    this.worker = null;

    this.callbacks = new Map();
    this.lastCallbackId = 0;
    this.idleTimeoutId = null;
  }

  async _execute(method, args = []) {
    if (gShutdown) {
      throw new Error("Remote Settings has shut down.");
    }
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
    if (!this.callbacks.size) {
      if (gShutdown) {
        this.stop();
        if (gShutdownResolver) {
          gShutdownResolver();
        }
      } else {
        this.idleTimeoutId = setTimeout(() => {
          this.stop();
        }, gMaxIdleMilliseconds);
      }
    }
  }

  stop() {
    this.worker.terminate();
    this.worker = null;
    this.idleTimeoutId = null;
  }

  async canonicalStringify(localRecords, remoteRecords, timestamp) {
    return this._execute("canonicalStringify", [
      localRecords,
      remoteRecords,
      timestamp,
    ]);
  }

  async importJSONDump(bucket, collection) {
    return this._execute("importJSONDump", [bucket, collection]);
  }

  async checkFileHash(filepath, size, hash) {
    return this._execute("checkFileHash", [filepath, size, hash]);
  }

  async checkContentHash(buffer, size, hash) {
    return this._execute("checkContentHash", [buffer, size, hash]);
  }
}

// Now, first add a shutdown blocker. If that fails, we must have
// shut down already.
// We're doing this here rather than in the Worker constructor because in
// principle having just 1 shutdown blocker for the entire file should be
// fine. If we ever start creating more than one Worker instance, this
// code will need adjusting to deal with that.
try {
  AsyncShutdown.profileBeforeChange.addBlocker(
    "Remote Settings profile-before-change",
    async () => {
      // First, indicate we've shut down.
      gShutdown = true;
      // Then, if we have no worker or no callbacks, we're done.
      if (
        !RemoteSettingsWorker.worker ||
        !RemoteSettingsWorker.callbacks.size
      ) {
        return null;
      }
      // Otherwise, return a promise that the worker will resolve.
      return new Promise(resolve => {
        gShutdownResolver = resolve;
      });
    },
    {
      fetchState() {
        return `Remaining: ${RemoteSettingsWorker.callbacks.size} callbacks.`;
      },
    }
  );
} catch (ex) {
  Cu.reportError(
    "Couldn't add shutdown blocker, assuming shutdown has started."
  );
  Cu.reportError(ex);
  // If AsyncShutdown throws, `profileBeforeChange` has already fired. Ignore it
  // and mark shutdown. Constructing the worker will report an error and do
  // nothing.
  gShutdown = true;
}

var RemoteSettingsWorker = new Worker(
  "resource://services-settings/RemoteSettingsWorker.js"
);
