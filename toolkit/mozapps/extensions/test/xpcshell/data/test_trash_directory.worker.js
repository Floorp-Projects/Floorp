/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/. */

/* import-globals-from /toolkit/components/workerloader/require.js */
importScripts("resource://gre/modules/workers/require.js");

const PromiseWorker = require("resource://gre/modules/workers/PromiseWorker.js");

class OpenFileWorker extends PromiseWorker.AbstractWorker {
  constructor() {
    super();

    this._file = null;
  }

  postMessage(message, ...transfers) {
    self.postMessage(message, transfers);
  }

  dispatch(method, args) {
    return this[method](...args);
  }

  open(path) {
    this._file = IOUtils.openFileForSyncReading(path);
  }

  close() {
    if (this._file) {
      this._file.close();
    }
  }
}

const worker = new OpenFileWorker();

self.addEventListener("message", msg => worker.handleMessage(msg));
self.addEventListener("unhandledrejection", err => {
  throw err.reason;
});
