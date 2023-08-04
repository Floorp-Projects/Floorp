/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/. */

/* import-globals-from /toolkit/components/workerloader/require.js */
importScripts("resource://gre/modules/workers/require.js");

const PromiseWorker = require("resource://gre/modules/workers/PromiseWorker.js");

const Agent = {
  _file: null,
  open(path) {
    this._file = IOUtils.openFileForSyncReading(path);
  },
  close() {
    this._file.close();
  },
};

// This boilerplate connects the PromiseWorker to the Agent so
// that messages from the main thread map to methods on the
// Agent.
const worker = new PromiseWorker.AbstractWorker();
worker.dispatch = function (method, args = []) {
  return Agent[method](...args);
};
worker.postMessage = function (result, ...transfers) {
  self.postMessage(result, ...transfers);
};
worker.close = function () {
  self.close();
};
self.addEventListener("message", msg => worker.handleMessage(msg));
self.addEventListener("unhandledrejection", function (error) {
  throw error.reason;
});
