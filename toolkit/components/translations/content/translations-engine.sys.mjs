/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
/* globals TE_addProfilerMarker, TE_getLogLevel, TE_log, TE_logError, TE_getLogLevel,
           TE_destroyEngineProcess, TE_requestEnginePayload, TE_reportEngineStatus,
           TE_resolveForceShutdown */

/**
 * This file lives in the translation engine's process and is in charge of managing the
 * lifecycle of the translations engines. This process is a singleton Web Content
 * process that can be created and destroyed as needed.
 *
 * The goal of the code in this file is to be as unprivileged as possible, which should
 * unlock Bug 1813789, which will make this file fully unprivileged.
 *
 * Each translation needs an engine for that specific translation pair. This engine is
 * kept around as long as the CACHE_TIMEOUT_MS, after this if some keepAlive event does
 * not happen, the engine is destroyed. An engine may be destroyed even when a page is
 * still open and may need translations in the future. This is handled gracefully by
 * creating new engines and MessagePorts on the fly.
 *
 * The engine communicates directly with the content page via a MessagePort. Each end
 * of the port is transfered from the parent process to the content process, and this
 * engine process. This port is transitory, and may be closed at any time. Only when a
 * translation has been requested once (which is initiated by the parent process) can
 * the content process re-request translation ports. This ensures a rogue content process
 * only has the capabilities to perform tasks that the parent process has given it.
 *
 * The messaging flow can get a little convoluted to handle all of the correctness cases,
 * but ideally communication passes through the message port as much as possible. There
 * are many scenarios such as:
 *
 *  - Translation pages becoming idle
 *  - Tab changing causing "pageshow" and "pagehide" visibility changes
 *  - Translation actor destruction (this can happen long after the page has been
 *                                   navigated away from, but is still alive in the
 *                                   page history)
 *  - Error states
 *  - Engine Process being graceful shut down (no engines left)
 *  - Engine Process being killed by the OS.
 *
 * The following is a diagram that attempts to illustrate the structure of the processes
 * and the communication channels that exist between them.
 *
 * ┌─────────────────────────────────────────────────────────────┐
 * │ PARENT PROCESS                                              │
 * │                                                             │
 * │  [TranslationsParent]  ←────→  [TranslationsEngineParent]   │
 * │                  ↑                                    ↑     │
 * └──────────────────│────────────────────────────────────│─────┘
 *                    │ JSWindowActor IPC calls            │ JSWindowActor IPC calls
 *                    │                                    │
 * ┌──────────────────│────────┐                     ┌─────│─────────────────────────────┐
 * │ CONTENT PROCESS  │        │                     │     │    ENGINE PROCESS           │
 * │                  │        │                     │     ↓                             │
 * │  [french.html]   │        │                     │ [TranslationsEngineChild]         │
 * │        ↕         ↓        │                     │            ↕                      │
 * │  [TranslationsChild]      │                     │ [translations-engine.html]        │
 * │  └──TranslationsDocument  │                     │    ├── "fr to en" engine          │
 * │     └──port1     « ═══════════ MessageChannel ════ » │   └── port2                  │
 * │                           │                     │    └── "de to en" engine (idle)   │
 * └───────────────────────────┘                     └───────────────────────────────────┘
 */

// How long the cache remains alive between uses, in milliseconds. In automation the
// engine is manually created and destroyed to avoid timing issues.
const CACHE_TIMEOUT_MS = 15_000;

/**
 * @typedef {import("./translations-document.sys.mjs").TranslationsDocument} TranslationsDocument
 * @typedef {import("../translations.js").TranslationsEnginePayload} TranslationsEnginePayload
 */

/**
 * The TranslationsEngine encapsulates the logic for translating messages. It can
 * only be set up for a single language translation pair. In order to change languages
 * a new engine should be constructed.
 *
 * The actual work for the translations happens in a worker. This class manages
 * instantiating and messaging the worker.
 *
 * Keep unused engines around in the TranslationsEngine.#cachedEngine cache in case
 * page navigation happens and we can re-use previous engines. The engines are very
 * heavy-weight, so get rid of them after a timeout. Once all are destroyed the
 * TranslationsEngineParent is notified that it can be destroyed.
 */
export class TranslationsEngine {
  /**
   * Maps a language pair key to a cached engine. Engines are kept around for a timeout
   * before they are removed so that they can be re-used during navigation.
   *
   * @type {Map<string, Promise<TranslationsEngine>>}
   */
  static #cachedEngines = new Map();

  /** @type {TimeoutID | null} */
  #keepAliveTimeout = null;

  /** @type {Worker} */
  #worker;

  /**
   * Multiple messages can be sent before a response is received. This ID is used to keep
   * track of the messages. It is incremented on every use.
   */
  #messageId = 0;

  /**
   * Returns a getter function that will create a translations engine on the first
   * call, and then return the cached one. After a timeout when the engine hasn't
   * been used, it is destroyed.
   *
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @param {number} innerWindowId
   * @returns {Promise<TranslationsEngine>}
   */
  static getOrCreate(fromLanguage, toLanguage, innerWindowId) {
    const languagePairKey = getLanguagePairKey(fromLanguage, toLanguage);
    let enginePromise = TranslationsEngine.#cachedEngines.get(languagePairKey);

    if (enginePromise) {
      return enginePromise;
    }

    TE_log(`Creating a new engine for "${fromLanguage}" to "${toLanguage}".`);

    // A new engine needs to be created.
    enginePromise = TranslationsEngine.create(
      fromLanguage,
      toLanguage,
      innerWindowId
    );

    TranslationsEngine.#cachedEngines.set(languagePairKey, enginePromise);

    enginePromise.catch(error => {
      TE_reportEngineStatus(innerWindowId, "error");
      TE_logError(
        `The engine failed to load for translating "${fromLanguage}" to "${toLanguage}". Removing it from the cache.`,
        error
      );
      // Remove the engine if it fails to initialize.
      TranslationsEngine.#removeEngineFromCache(languagePairKey);
    });

    return enginePromise;
  }

  /**
   * Removes the engine, and if it's the last, call the process to destroy itself.
   * @param {string} languagePairKey
   * @param {boolean} force - On forced shutdowns, it's not necessary to notify the
   *                          parent process.
   */
  static #removeEngineFromCache(languagePairKey, force) {
    TranslationsEngine.#cachedEngines.delete(languagePairKey);
    if (TranslationsEngine.#cachedEngines.size === 0 && !force) {
      TE_log("The last engine was removed, destroying this process.");
      TE_destroyEngineProcess();
    }
  }

  /**
   * Create a TranslationsEngine and bypass the cache.
   *
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @param {number} innerWindowId
   * @returns {Promise<TranslationsEngine>}
   */
  static async create(fromLanguage, toLanguage, innerWindowId) {
    const startTime = performance.now();

    const engine = new TranslationsEngine(
      fromLanguage,
      toLanguage,
      await TE_requestEnginePayload(fromLanguage, toLanguage)
    );

    await engine.isReady;

    TE_addProfilerMarker({
      startTime,
      message: `Translations engine loaded for "${fromLanguage}" to "${toLanguage}"`,
      innerWindowId,
    });

    return engine;
  }

  /**
   * Signal to the engines that they are being forced to shutdown.
   */
  static forceShutdown() {
    return Promise.allSettled(
      [...TranslationsEngine.#cachedEngines].map(
        async ([langPair, enginePromise]) => {
          TE_log(`Force shutdown of the engine "${langPair}"`);
          const engine = await enginePromise;
          engine.terminate(true /* force */);
        }
      )
    );
  }

  /**
   * Terminates the engine and its worker after a timeout.
   * @param {boolean} force
   */
  terminate = (force = false) => {
    const message = `Terminating translations engine "${this.languagePairKey}".`;
    TE_addProfilerMarker({ message });
    TE_log(message);
    this.#worker.terminate();
    this.#worker = null;
    if (this.#keepAliveTimeout) {
      clearTimeout(this.#keepAliveTimeout);
    }
    for (const [innerWindowId, data] of ports) {
      const { fromLanguage, toLanguage, port } = data;
      if (
        fromLanguage === this.fromLanguage &&
        toLanguage === this.toLanguage
      ) {
        // This port is still active but being closed.
        ports.delete(innerWindowId);
        port.postMessage({ type: "TranslationsPort:EngineTerminated" });
        port.close();
      }
    }
    TranslationsEngine.#removeEngineFromCache(this.languagePairKey, force);
  };

  /**
   * The worker needs to be shutdown after some amount of time of not being used.
   */
  keepAlive() {
    if (this.#keepAliveTimeout) {
      // Clear any previous timeout.
      clearTimeout(this.#keepAliveTimeout);
    }
    // In automated tests, the engine is manually destroyed.
    if (!Cu.isInAutomation) {
      this.#keepAliveTimeout = setTimeout(this.terminate, CACHE_TIMEOUT_MS);
    }
  }

  /**
   * Construct and initialize the worker.
   *
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @param {TranslationsEnginePayload} enginePayload - If there is no engine payload
   *   then the engine will be mocked. This allows this class to be used in tests.
   */
  constructor(fromLanguage, toLanguage, enginePayload) {
    /** @type {string} */
    this.fromLanguage = fromLanguage;
    /** @type {string} */
    this.toLanguage = toLanguage;
    this.languagePairKey = getLanguagePairKey(fromLanguage, toLanguage);
    this.#worker = new Worker(
      "chrome://global/content/translations/translations-engine-worker.js"
    );

    /** @type {Promise<void>} */
    this.isReady = new Promise((resolve, reject) => {
      const onMessage = ({ data }) => {
        TE_log("Received initialization message", data);
        if (data.type === "initialization-success") {
          resolve();
        } else if (data.type === "initialization-error") {
          reject(data.error);
        }
        this.#worker.removeEventListener("message", onMessage);
      };
      this.#worker.addEventListener("message", onMessage);

      // Schedule the first timeout for keeping the engine alive.
      this.keepAlive();
    });

    // Make sure the ArrayBuffers are transferred, not cloned.
    // https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API/Transferable_objects
    const transferables = [];
    if (enginePayload) {
      transferables.push(enginePayload.bergamotWasmArrayBuffer);
      for (const files of enginePayload.languageModelFiles) {
        for (const { buffer } of Object.values(files)) {
          transferables.push(buffer);
        }
      }
    }

    this.#worker.postMessage(
      {
        type: "initialize",
        fromLanguage,
        toLanguage,
        enginePayload,
        messageId: this.#messageId++,
        logLevel: TE_getLogLevel(),
      },
      transferables
    );
  }

  /**
   * The implementation for translation. Use translateText or translateHTML for the
   * public API.
   *
   * @param {string} sourceText
   * @param {boolean} isHTML
   * @param {number} innerWindowId
   * @returns {Promise<string[]>}
   */
  translate(sourceText, isHTML, innerWindowId) {
    this.keepAlive();

    const messageId = this.#messageId++;

    return new Promise((resolve, reject) => {
      const onMessage = ({ data }) => {
        if (
          data.type === "translations-discarded" &&
          data.innerWindowId === innerWindowId
        ) {
          // The page was unloaded, and we no longer need to listen for a response.
          this.#worker.removeEventListener("message", onMessage);
          return;
        }

        if (data.messageId !== messageId) {
          // Multiple translation requests can be sent before a response is received.
          // Ensure that the response received here is the correct one.
          return;
        }

        if (data.type === "translation-response") {
          // Also keep the translation alive after getting a result, as many translations
          // can queue up at once, and then it can take minutes to resolve them all.
          this.keepAlive();
          resolve(data.targetText);
        }
        if (data.type === "translation-error") {
          reject(data.error);
        }
        this.#worker.removeEventListener("message", onMessage);
      };

      this.#worker.addEventListener("message", onMessage);

      this.#worker.postMessage({
        type: "translation-request",
        isHTML,
        sourceText,
        messageId,
        innerWindowId,
      });
    });
  }

  /**
   * Applies a function only if a cached engine exists.
   *
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @param {(engine: TranslationsEngine) => void} fn
   */
  static withCachedEngine(fromLanguage, toLanguage, fn) {
    const engine = TranslationsEngine.#cachedEngines.get(
      getLanguagePairKey(fromLanguage, toLanguage)
    );

    if (engine) {
      engine.then(fn).catch(() => {});
    }
  }

  /**
   * Stop processing the translation queue. All in-progress messages will be discarded.
   *
   * @param {number} innerWindowId
   */
  discardTranslationQueue(innerWindowId) {
    this.#worker.postMessage({
      type: "discard-translation-queue",
      innerWindowId,
    });
  }

  /**
   * Pause or resume the translations from a cached engine.
   *
   * @param {boolean} pause
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @param {number} innerWindowId
   */
  static pause(pause, fromLanguage, toLanguage, innerWindowId) {
    TranslationsEngine.withCachedEngine(fromLanguage, toLanguage, engine => {
      engine.pause(pause, innerWindowId);
    });
  }
}

/**
 * Creates a lookup key that is unique to each fromLanguage-toLanguage pair.
 *
 * @param {string} fromLanguage
 * @param {string} toLanguage
 * @returns {string}
 */
function getLanguagePairKey(fromLanguage, toLanguage) {
  return `${fromLanguage},${toLanguage}`;
}

/**
 * Maps the innerWindowId to the port.
 * @type {Map<number, { fromLanguage: string, toLanguage: string, port: MessagePort }}
 */
const ports = new Map();

/**
 * Listen to the port to the content process for incoming messages, and pass
 * them to the TranslationsEngine manager. The other end of the port is held
 * in the content process by the TranslationsDocument.
 * @param {string} fromLanguage
 * @param {string} toLanguage
 * @param {number} innerWindowId
 * @param {MessagePort} port
 */
function listenForPortMessages(fromLanguage, toLanguage, innerWindowId, port) {
  let isFirstLoad = true;

  async function handleMessage({ data }) {
    switch (data.type) {
      case "TranslationsPort:GetEngineStatusRequest": {
        // This message gets sent first before the translation queue is processed.
        // The engine is most likely to fail on the initial invocation. Any failure
        // past the first one is not reported to the UI.
        TranslationsEngine.getOrCreate(
          fromLanguage,
          toLanguage,
          innerWindowId
        ).then(
          () => {
            port.postMessage({
              type: "TranslationsPort:GetEngineStatusResponse",
              status: "ready",
            });
          },
          () => {
            port.postMessage({
              type: "TranslationsPort:GetEngineStatusResponse",
              status: "error",
            });
            // After an error no more translation requests will be sent. Go ahead
            // and close the port.
            port.close();
            ports.delete(innerWindowId);
          }
        );
        break;
      }
      case "TranslationsPort:TranslationRequest": {
        const { sourceText, isHTML, messageId } = data;
        const engine = await TranslationsEngine.getOrCreate(
          fromLanguage,
          toLanguage,
          innerWindowId
        );
        const targetText = await engine.translate(
          sourceText,
          isHTML,
          innerWindowId
        );
        if (isFirstLoad) {
          isFirstLoad = false;
          TE_log("The engine is ready for translations.", { innerWindowId });
          TE_reportEngineStatus(innerWindowId, "ready");
        }
        port.postMessage({
          type: "TranslationsPort:TranslationResponse",
          messageId,
          targetText,
        });
        break;
      }
      case "TranslationsPort:DiscardTranslations": {
        discardTranslations(innerWindowId);
        break;
      }
      default:
        TE_logError("Unknown translations port message: " + data.type);
        break;
    }
  }

  if (port.onmessage) {
    TE_logError(
      new Error("The MessagePort onmessage handler was already present.")
    );
  }

  port.onmessage = event => {
    handleMessage(event).catch(error => TE_logError(error));
  };
}

/**
 * Discards the queue and removes the port.
 *
 * @param {innerWindowId} number
 */
function discardTranslations(innerWindowId) {
  TE_log("Discarding translations, innerWindowId:", innerWindowId);

  const portData = ports.get(innerWindowId);
  if (portData) {
    const { port, fromLanguage, toLanguage } = portData;
    port.close();
    ports.delete(innerWindowId);

    TranslationsEngine.withCachedEngine(fromLanguage, toLanguage, engine => {
      engine.discardTranslationQueue(innerWindowId);
    });
  }
}

/**
 * Listen for events coming from the TranslationsEngine actor.
 */
window.addEventListener("message", ({ data }) => {
  switch (data.type) {
    case "StartTranslation": {
      const { fromLanguage, toLanguage, innerWindowId, port } = data;
      TE_log("Starting translation", innerWindowId);
      listenForPortMessages(fromLanguage, toLanguage, innerWindowId, port);
      ports.set(innerWindowId, { port, fromLanguage, toLanguage });
      break;
    }
    case "DiscardTranslations": {
      const { innerWindowId } = data;
      discardTranslations(innerWindowId);
      break;
    }
    case "ForceShutdown": {
      TranslationsEngine.forceShutdown().then(() => {
        TE_resolveForceShutdown();
      });
      break;
    }
    default:
      throw new Error("Unknown TranslationsEngineChromeToContent event.");
  }
});
