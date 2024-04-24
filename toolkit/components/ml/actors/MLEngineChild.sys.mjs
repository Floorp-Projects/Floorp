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
  ModelHub: "chrome://global/content/ml/ModelHub.sys.mjs",
  PipelineOptions: "chrome://global/content/ml/EngineProcess.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.ml.logLevel",
    prefix: "ML",
  });
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "CACHE_TIMEOUT_MS",
  "browser.ml.modelCacheTimeout"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "MODEL_HUB_ROOT_URL",
  "browser.ml.modelHubRootUrl"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "MODEL_HUB_URL_TEMPLATE",
  "browser.ml.modelHubUrlTemplate"
);
XPCOMUtils.defineLazyPreferenceGetter(lazy, "LOG_LEVEL", "browser.ml.logLevel");

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
        const { port, pipelineOptions } = data;

        // Override some options using prefs
        let options = new lazy.PipelineOptions(pipelineOptions);

        options.updateOptions({
          modelHubRootUrl: lazy.MODEL_HUB_ROOT_URL,
          modelHubUrlTemplate: lazy.MODEL_HUB_URL_TEMPLATE,
          timeoutMS: lazy.CACHE_TIMEOUT_MS,
          logLevel: lazy.LOG_LEVEL,
        });

        this.#engineDispatchers.set(
          options.taskName,
          new EngineDispatcher(this, port, options)
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
   * Gets the wasm array buffer from RemoteSettings.
   *
   * @returns {Promise<ArrayBuffer>}
   */
  getWasmArrayBuffer() {
    return this.sendQuery("MLEngine:GetWasmArrayBuffer");
  }

  /**
   * Gets the inference options from RemoteSettings.
   *
   * @returns {Promise<object>}
   */
  getInferenceOptions(taskName) {
    return this.sendQuery(`MLEngine:GetInferenceOptions:${taskName}`);
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
  #taskName;

  /** Creates the inference engine given the wasm runtime and the run options.
   *
   * The initialization is done in three steps:
   * 1. The wasm runtime is fetched from RS
   * 2. The inference options are fetched from RS and augmented with the pipeline options.
   * 3. The inference engine is created with the wasm runtime and the options.
   *
   * Any exception here will be bubbled up for the constructor to log.
   *
   * @param {PipelineOptions} pipelineOptions
   * @returns {Promise<Engine>}
   */
  async initializeInferenceEngine(pipelineOptions) {
    // Create the inference engine given the wasm runtime and the options.
    const wasm = await this.mlEngineChild.getWasmArrayBuffer();
    const inferenceOptions = await this.mlEngineChild.getInferenceOptions(
      this.#taskName
    );
    lazy.console.debug("Inference engine options:", inferenceOptions);
    pipelineOptions.updateOptions(inferenceOptions);

    return InferenceEngine.create(wasm, pipelineOptions);
  }

  /**
   * @param {MLEngineChild} mlEngineChild
   * @param {MessagePort} port
   * @param {PipelineOptions} pipelineOptions
   */
  constructor(mlEngineChild, port, pipelineOptions) {
    this.mlEngineChild = mlEngineChild;
    this.#taskName = pipelineOptions.taskName;
    this.timeoutMS = pipelineOptions.timeoutMS;

    this.#engine = this.initializeInferenceEngine(pipelineOptions);

    // Trigger the keep alive timer.
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
    this.mlEngineChild.removeEngine(this.#taskName);
    try {
      const engine = await this.#engine;
      engine.terminate();
    } catch (error) {
      lazy.console.error("Failed to get the engine", error);
    }
  }
}

let modelHub = null; // This will hold the ModelHub instance to reuse it.

/**
 * Retrieves a model file as an ArrayBuffer from the specified URL.
 * This function normalizes the URL, extracts the organization, model name, and file path,
 * then fetches the model file using the ModelHub API. The `modelHub` instance is created
 * only once and reused for subsequent calls to optimize performance.
 *
 * @param {string} url - The URL of the model file to fetch. Can be a path relative to
 * the model hub root or an absolute URL.
 * @returns {Promise} A promise that resolves to a Meta object containing the URL, response headers,
 * and data as an ArrayBuffer. The data is marked for transfer to avoid cloning.
 */
async function getModelFile(url) {
  // Create the model hub instance if needed
  if (!modelHub) {
    lazy.console.debug("Creating model hub instance");
    modelHub = new lazy.ModelHub({
      rootUrl: lazy.MODEL_HUB_ROOT_URL,
      urlTemplate: lazy.MODEL_HUB_URL_TEMPLATE,
    });
  }

  if (url.startsWith(lazy.MODEL_HUB_ROOT_URL)) {
    url = url.slice(lazy.MODEL_HUB_ROOT_URL.length);
    // Make sure we get a front slash
    if (!url.startsWith("/")) {
      url = `/${url}`;
    }
  }

  // Parsing url to get model name, and file path.
  // if this errors out, it will be caught in the worker
  const parsedUrl = modelHub.parseUrl(url);

  let [data, headers] = await modelHub.getModelFileAsArrayBuffer(parsedUrl);
  return new lazy.BasePromiseWorker.Meta([url, headers, data], {
    transfers: [data],
  });
}

/**
 * Wrapper around the ChromeWorker that runs the inference.
 */
class InferenceEngine {
  /** @type {BasePromiseWorker} */
  #worker;

  /**
   * Initialize the worker.
   *
   * @param {ArrayBuffer} wasm
   * @param {PipelineOptions} pipelineOptions
   * @returns {InferenceEngine}
   */
  static async create(wasm, pipelineOptions) {
    /** @type {BasePromiseWorker} */
    const worker = new lazy.BasePromiseWorker(
      "chrome://global/content/ml/MLEngine.worker.mjs",
      { type: "module" },
      { getModelFile }
    );

    const args = [wasm, pipelineOptions];
    const closure = {};
    const transferables = [wasm];
    await worker.post("initializeEngine", args, closure, transferables);
    return new InferenceEngine(worker);
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
