/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

/**
 * @typedef {import("../../promiseworker/PromiseWorker.sys.mjs").BasePromiseWorker} BasePromiseWorker
 */

/**
 * @typedef {object} Lazy
 * @property {typeof import("../../promiseworker/PromiseWorker.sys.mjs").BasePromiseWorker} BasePromiseWorker
 * @property {typeof setTimeout} setTimeout
 * @property {typeof clearTimeout} clearTimeout
 */

/** @type {Lazy} */
const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  BasePromiseWorker: "resource://gre/modules/PromiseWorker.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.ml.logLevel",
    prefix: "ML",
  });
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "loggingLevel",
  "browser.ml.logLevel"
);

/**
 * The engine child is responsible for the life cycle and instantiation of the local
 * machine learning inference engine.
 */
export class MLEngineChild extends JSWindowActorChild {
  /**
   * The cached engines.
   *
   * @type {Map<string, EngineDispatcher>}
   */
  #engineDispatchers = new Map();

  // eslint-disable-next-line consistent-return
  async receiveMessage({ name, data }) {
    switch (name) {
      case "MLEngine:NewPort": {
        const { engineName, port, timeoutMS } = data;
        this.#engineDispatchers.set(
          engineName,
          new EngineDispatcher(this, port, engineName, timeoutMS)
        );
        break;
      }
      case "MLEngine:ForceShutdown": {
        for (const engineDispatcher of this.#engineDispatchers.values()) {
          return engineDispatcher.terminate();
        }
        this.#engineDispatchers = null;
        break;
      }
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "DOMContentLoaded":
        this.sendAsyncMessage("MLEngine:Ready");
        break;
    }
  }

  /**
   * @returns {ArrayBuffer}
   */
  getWasmArrayBuffer() {
    return this.sendQuery("MLEngine:GetWasmArrayBuffer");
  }

  /**
   * @param {string} engineName
   */
  removeEngine(engineName) {
    this.#engineDispatchers.delete(engineName);
    if (this.#engineDispatchers.size === 0) {
      this.sendQuery("MLEngine:DestroyEngineProcess");
    }
  }
}

/**
 * This classes manages the lifecycle of an ML Engine, and handles dispatching messages
 * to it.
 */
class EngineDispatcher {
  /** @type {Set<MessagePort>} */
  #ports = new Set();

  /** @type {TimeoutID | null} */
  #keepAliveTimeout = null;

  /** @type {PromiseWithResolvers} */
  #modelRequest;

  /** @type {Promise<Engine> | null} */
  #engine = null;

  /** @type {string} */
  #engineName;

  /**
   * @param {MLEngineChild} mlEngineChild
   * @param {MessagePort} port
   * @param {string} engineName
   * @param {number} timeoutMS
   */
  constructor(mlEngineChild, port, engineName, timeoutMS) {
    /** @type {MLEngineChild} */
    this.mlEngineChild = mlEngineChild;

    /** @type {number} */
    this.timeoutMS = timeoutMS;

    this.#engineName = engineName;

    this.#engine = Promise.all([
      this.mlEngineChild.getWasmArrayBuffer(),
      this.getModel(port),
    ]).then(([wasm, model]) => FakeEngine.create(wasm, model));

    this.#engine
      .then(() => void this.keepAlive())
      .catch(error => {
        if (
          // Ignore errors from tests intentionally causing errors.
          !error?.message?.startsWith("Intentionally")
        ) {
          lazy.console.error("Could not initalize the engine", error);
        }
      });

    this.setupMessageHandler(port);
  }

  /**
   * The worker needs to be shutdown after some amount of time of not being used.
   */
  keepAlive() {
    if (this.#keepAliveTimeout) {
      // Clear any previous timeout.
      lazy.clearTimeout(this.#keepAliveTimeout);
    }
    // In automated tests, the engine is manually destroyed.
    if (!Cu.isInAutomation) {
      this.#keepAliveTimeout = lazy.setTimeout(this.terminate, this.timeoutMS);
    }
  }

  /**
   * @param {MessagePort} port
   */
  getModel(port) {
    if (this.#modelRequest) {
      // There could be a race to get a model, use the first request.
      return this.#modelRequest.promise;
    }
    this.#modelRequest = Promise.withResolvers();
    port.postMessage({ type: "EnginePort:ModelRequest" });
    return this.#modelRequest.promise;
  }

  /**
   * @param {MessagePort} port
   */
  setupMessageHandler(port) {
    port.onmessage = async ({ data }) => {
      switch (data.type) {
        case "EnginePort:Discard": {
          port.close();
          this.#ports.delete(port);
          break;
        }
        case "EnginePort:Terminate": {
          this.terminate();
          break;
        }
        case "EnginePort:ModelResponse": {
          if (this.#modelRequest) {
            const { model, error } = data;
            if (model) {
              this.#modelRequest.resolve(model);
            } else {
              this.#modelRequest.reject(error);
            }
            this.#modelRequest = null;
          } else {
            lazy.console.error(
              "Got a EnginePort:ModelResponse but no model resolvers"
            );
          }
          break;
        }
        case "EnginePort:Run": {
          const { requestId, request } = data;
          let engine;
          try {
            engine = await this.#engine;
          } catch (error) {
            port.postMessage({
              type: "EnginePort:RunResponse",
              requestId,
              response: null,
              error,
            });
            // The engine failed to load. Terminate the entire dispatcher.
            this.terminate();
            return;
          }

          // Do not run the keepAlive timer until we are certain that the engine loaded,
          // as the engine shouldn't be killed while it is initializing.
          this.keepAlive();

          try {
            port.postMessage({
              type: "EnginePort:RunResponse",
              requestId,
              response: await engine.run(request),
              error: null,
            });
          } catch (error) {
            port.postMessage({
              type: "EnginePort:RunResponse",
              requestId,
              response: null,
              error,
            });
          }
          break;
        }
        default:
          lazy.console.error("Unknown port message to engine: ", data);
          break;
      }
    };
  }

  /**
   * Terminates the engine and its worker after a timeout.
   */
  async terminate() {
    if (this.#keepAliveTimeout) {
      lazy.clearTimeout(this.#keepAliveTimeout);
      this.#keepAliveTimeout = null;
    }
    for (const port of this.#ports) {
      port.postMessage({ type: "EnginePort:EngineTerminated" });
      port.close();
    }
    this.#ports = new Set();
    this.mlEngineChild.removeEngine(this.#engineName);
    try {
      const engine = await this.#engine;
      engine.terminate();
    } catch (error) {
      console.error("Failed to get the engine", error);
    }
  }
}

/**
 * Fake the engine by slicing the text in half.
 */
class FakeEngine {
  /** @type {BasePromiseWorker} */
  #worker;

  /**
   * Initialize the worker.
   *
   * @param {ArrayBuffer} wasm
   * @param {ArrayBuffer} model
   * @returns {FakeEngine}
   */
  static async create(wasm, model) {
    /** @type {BasePromiseWorker} */
    const worker = new lazy.BasePromiseWorker(
      "chrome://global/content/ml/MLEngine.worker.mjs",
      { type: "module" }
    );

    const args = [wasm, model, lazy.loggingLevel];
    const closure = {};
    const transferables = [wasm, model];
    await worker.post("initializeEngine", args, closure, transferables);
    return new FakeEngine(worker);
  }

  /**
   * @param {BasePromiseWorker} worker
   */
  constructor(worker) {
    this.#worker = worker;
  }

  /**
   * @param {string} request
   * @returns {Promise<string>}
   */
  run(request) {
    return this.#worker.post("run", [request]);
  }

  terminate() {
    this.#worker.terminate();
    this.#worker = null;
  }
}
