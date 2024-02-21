/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { PromiseWorker } from "resource://gre/modules/workers/PromiseWorker.mjs";

// Respect the preference "browser.ml.logLevel".
let _loggingLevel = "Error";
function log(...args) {
  if (_loggingLevel !== "Error" && _loggingLevel !== "Warn") {
    console.log("ML:", ...args);
  }
}
function trace(...args) {
  if (_loggingLevel === "Trace" || _loggingLevel === "All") {
    console.log("ML:", ...args);
  }
}

/**
 * The actual MLEngine lives here in a worker.
 */
class MLEngineWorker {
  /** @type {ArrayBuffer} */
  #wasm;
  /** @type {ArrayBuffer} */
  #model;

  constructor() {
    // Connect the provider to the worker.
    this.#connectToPromiseWorker();
  }

  /**
   * @param {ArrayBuffer} wasm
   * @param {ArrayBuffer} model
   * @param {string} loggingLevel
   */
  initializeEngine(wasm, model, loggingLevel) {
    this.#wasm = wasm;
    this.#model = model;
    _loggingLevel = loggingLevel;
    // TODO - Initialize the engine for real here.
    log("MLEngineWorker is initalized");
  }

  /**
   * Run the worker.
   *
   * @param {string} request
   */
  run(request) {
    if (!this.#wasm) {
      throw new Error("Expected the wasm to exist.");
    }
    if (!this.#model) {
      throw new Error("Expected the model to exist");
    }
    if (request === "throw") {
      throw new Error(
        'Received the message "throw", so intentionally throwing an error.'
      );
    }
    trace("inference run requested with:", request);
    return request.slice(0, Math.floor(request.length / 2));
  }

  /**
   * Glue code to connect the `MLEngineWorker` to the PromiseWorker interface.
   */
  #connectToPromiseWorker() {
    const worker = new PromiseWorker.AbstractWorker();
    worker.dispatch = (method, args = []) => {
      if (!this[method]) {
        throw new Error("Method does not exist: " + method);
      }
      return this[method](...args);
    };
    worker.close = () => self.close();
    worker.postMessage = (message, ...transfers) => {
      self.postMessage(message, ...transfers);
    };

    self.addEventListener("message", msg => worker.handleMessage(msg));
    self.addEventListener("unhandledrejection", function (error) {
      throw error.reason;
    });
  }
}

new MLEngineWorker();
