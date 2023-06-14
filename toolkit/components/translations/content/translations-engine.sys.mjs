/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "logLevel",
  "browser.translations.logLevel"
);

XPCOMUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.translations.logLevel",
    prefix: "Translations",
  });
});

ChromeUtils.defineESModuleGetters(lazy, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
});

// How long the cache remains alive between uses, in milliseconds.
const CACHE_TIMEOUT_MS = 10_000;

class TranslationsEngineCache {
  /** @type {Record<string, Promise<TranslationsEngine>>} */
  #engines = {};

  /** @type {Record<string, TimeoutID>} */
  #timeouts = {};

  /**
   * Returns a getter function that will create a translations engine on the first
   * call, and then return the cached one. After a timeout when the engine hasn't
   * been used, it is destroyed.
   *
   * @param {TranslationsChild} actor
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @returns {(() => Promise<TranslationsEngine>) | ((onlyFromCache: true) => Promise<TranslationsEngine | null>)}
   */
  createGetter(actor, fromLanguage, toLanguage) {
    return async (onlyFromCache = false) => {
      let enginePromise =
        this.#engines[
          TranslationsChild.languagePairKey(fromLanguage, toLanguage)
        ];
      if (enginePromise) {
        return enginePromise;
      }
      if (onlyFromCache) {
        return null;
      }

      // A new engine needs to be created.
      enginePromise = actor.createTranslationsEngine(fromLanguage, toLanguage);

      const key = TranslationsChild.languagePairKey(fromLanguage, toLanguage);
      this.#engines[key] = enginePromise;

      // Remove the engine if it fails to initialize.
      enginePromise.catch(error => {
        lazy.console.log(
          `The engine failed to load for translating "${fromLanguage}" to "${toLanguage}". Removing it from the cache.`,
          error
        );
        this.#engines[key] = null;
      });

      const engine = await enginePromise;

      // These methods will be spied on, and when called they will keep the engine alive.
      this.spyOn(engine, "translateText");
      this.spyOn(engine, "translateHTML");
      this.spyOn(engine, "discardTranslationQueue");
      this.keepAlive(fromLanguage, toLanguage);

      return engine;
    };
  }

  /**
   * Spies on a method, so that when it is called, the engine is kept alive.
   *
   * @param {TranslationsEngine} engine
   * @param {string} methodName
   */
  spyOn(engine, methodName) {
    const method = engine[methodName].bind(engine);
    engine[methodName] = (...args) => {
      this.keepAlive(engine.fromLanguage, engine.toLanguage);
      return method(...args);
    };
  }

  /**
   * @param {string} fromLanguage
   * @param {string} toLanguage
   */
  keepAlive(fromLanguage, toLanguage) {
    const key = TranslationsChild.languagePairKey(fromLanguage);
    const timeoutId = this.#timeouts[key];
    if (timeoutId) {
      lazy.clearTimeout(timeoutId);
    }
    const enginePromise = this.#engines[key];
    if (!enginePromise) {
      // It appears that the engine is already dead.
      return;
    }
    this.#timeouts[key] = lazy.setTimeout(() => {
      // Delete the caches.
      delete this.#engines[key];
      delete this.#timeouts[key];

      // Terminate the engine worker.
      enginePromise.then(engine => engine.terminate());
    }, CACHE_TIMEOUT_MS);
  }

  /**
   * Sees if an engine is still in the cache.
   */
  isInCache(fromLanguage, toLanguage) {
    this.keepAlive(fromLanguage, toLanguage);
    return Boolean(
      this.#engines[TranslationsChild.languagePairKey(fromLanguage, toLanguage)]
    );
  }
}

const translationsEngineCache = new TranslationsEngineCache();

/**
 * The TranslationsEngine encapsulates the logic for translating messages. It can
 * only be set up for a single language translation pair. In order to change languages
 * a new engine should be constructed.
 *
 * The actual work for the translations happens in a worker. This class manages
 * instantiating and messaging the worker.
 */
export class TranslationsEngine {
  /** @type {Worker} */
  #translationsWorker;
  // Multiple messages can be sent before a response is received. This ID is used to keep
  // track of the messages. It is incremented on every use.
  #messageId = 0;

  /**
   * Construct and initialize the worker.
   *
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @param {TranslationsEnginePayload} enginePayload - If there is no engine payload
   *   then the engine will be mocked. This allows this class to be used in tests.
   * @param {number} innerWindowId - This only used for creating profiler markers in
   *   the initial creation of the engine.
   */
  constructor(fromLanguage, toLanguage, enginePayload, innerWindowId) {
    /** @type {string} */
    this.fromLanguage = fromLanguage;
    /** @type {string} */
    this.toLanguage = toLanguage;
    this.#translationsWorker = new Worker(
      "chrome://global/content/translations/translations-engine-worker.js"
    );

    /** @type {Promise<void>} */
    this.isReady = new Promise((resolve, reject) => {
      const onMessage = ({ data }) => {
        lazy.console.log("Received initialization message", data);
        if (data.type === "initialization-success") {
          resolve();
        } else if (data.type === "initialization-error") {
          reject(data.error);
        }
        this.#translationsWorker.removeEventListener("message", onMessage);
      };
      this.#translationsWorker.addEventListener("message", onMessage);
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

    this.#translationsWorker.postMessage(
      {
        type: "initialize",
        fromLanguage,
        toLanguage,
        enginePayload,
        innerWindowId,
        messageId: this.#messageId++,
        logLevel: lazy.logLevel,
      },
      transferables
    );
  }

  /**
   * Translate text without any HTML.
   *
   * @param {string[]} messageBatch
   * @param {number} innerWindowId
   * @returns {Promise<string[]>}
   */
  translateText(messageBatch, innerWindowId) {
    return this.#translate(messageBatch, false, innerWindowId);
  }

  /**
   * Translate valid HTML. Note that this method throws if invalid markup is provided.
   *
   * @param {string[]} messageBatch
   * @param {number} innerWindowId
   * @returns {Promise<string[]>}
   */
  translateHTML(messageBatch, innerWindowId) {
    return this.#translate(messageBatch, true, innerWindowId);
  }

  /**
   * The implementation for translation. Use translateText or translateHTML for the
   * public API.
   *
   * @param {string[]} messageBatch
   * @param {boolean} isHTML
   * @param {number} innerWindowId
   * @returns {Promise<string[]>}
   */
  #translate(messageBatch, isHTML, innerWindowId) {
    const messageId = this.#messageId++;

    return new Promise((resolve, reject) => {
      const onMessage = ({ data }) => {
        if (
          data.type === "translations-discarded" &&
          data.innerWindowId === innerWindowId
        ) {
          // The page was unloaded, and we no longer need to listen for a response.
          this.#translationsWorker.removeEventListener("message", onMessage);
          return;
        }

        if (data.messageId !== messageId) {
          // Multiple translation requests can be sent before a response is received.
          // Ensure that the response received here is the correct one.
          return;
        }

        if (data.type === "translation-response") {
          resolve(data.translations);
        }
        if (data.type === "translation-error") {
          reject(data.error);
        }
        this.#translationsWorker.removeEventListener("message", onMessage);
      };

      this.#translationsWorker.addEventListener("message", onMessage);

      this.#translationsWorker.postMessage({
        type: "translation-request",
        isHTML,
        messageBatch,
        messageId,
        innerWindowId,
      });
    });
  }

  /**
   * The worker should be GCed just fine on its own, but go ahead and signal to
   * the worker that it's no longer needed. This will immediately cancel any in-progress
   * translations.
   */
  terminate() {
    this.#translationsWorker.terminate();
  }

  /**
   * Stop processing the translation queue. All in-progress messages will be discarded.
   *
   * @param {number} innerWindowId
   */
  discardTranslationQueue(innerWindowId) {
    ChromeUtils.addProfilerMarker(
      "TranslationsChild",
      null,
      "Request to discard translation queue"
    );
    this.#translationsWorker.postMessage({
      type: "discard-translation-queue",
      innerWindowId,
    });
  }
}
