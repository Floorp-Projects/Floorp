/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * @typedef {import("./translations-document.sys.mjs").TranslationsDocument} TranslationsDocument
 * @typedef {import("../translations.js").TranslationsEnginePayload} TranslationsEnginePayload
 */

const lazy = {};

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "logLevel",
  "browser.translations.logLevel"
);

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.translations.logLevel",
    prefix: "Translations",
  });
});

ChromeUtils.defineESModuleGetters(lazy, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  TranslationsDocument:
    "chrome://global/content/translations/translations-document.sys.mjs",
});

// How long the cache remains alive between uses, in milliseconds.
const CACHE_TIMEOUT_MS = 10_000;

/**
 * The TranslationsEngine encapsulates the logic for translating messages. It can
 * only be set up for a single language translation pair. In order to change languages
 * a new engine should be constructed.
 *
 * The actual work for the translations happens in a worker. This class manages
 * instantiating and messaging the worker.
 *
 * Keep 1 language engine around in the TranslationsEngine.#cachedEngine cache in case
 * page navigation happens and we can re-use the previous engine. The engines are very
 * heavy-weight, so we only want to keep one around at a time.
 */
export class TranslationsEngine {
  /** @type {null | { languagePairKey: string, enginePromise: Promise<TranslationsEngine> }} */
  static #cachedEngine = null;

  /** @type {null | TimeoutID} */
  static #keepAliveTimeout = null;

  /** @type {Worker} */
  #translationsWorker;

  /**
   * Multiple messages can be sent before a response is received. This ID is used to keep
   * track of the messages. It is incremented on every use.
   */
  #messageId = 0;

  /**
   * @type {TranslationsDocument | null}
   */
  translatedDoc = null;

  /**
   * Returns a getter function that will create a translations engine on the first
   * call, and then return the cached one. After a timeout when the engine hasn't
   * been used, it is destroyed.
   *
   * @param {TranslationsChild} actor
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @returns {Promise<TranslationsEngine | null>}
   */
  static getOrCreate(actor, fromLanguage, toLanguage) {
    const languagePairKey = getLanguagePairKey(fromLanguage, toLanguage);
    if (this.#cachedEngine?.languagePairKey === languagePairKey) {
      return this.#cachedEngine.enginePromise;
    }

    // A new engine needs to be created.
    const enginePromise = TranslationsEngine.create(
      actor,
      fromLanguage,
      toLanguage
    );

    this.#cachedEngine = { languagePairKey, enginePromise };
    TranslationsEngine.keepAlive(languagePairKey);

    enginePromise.then(
      () => {
        void TranslationsEngine.keepAlive(languagePairKey);
      },
      error => {
        lazy.console.error(
          `The engine failed to load for translating "${fromLanguage}" to "${toLanguage}". Removing it from the cache.`,
          error
        );
        // Remove the engine if it fails to initialize.
        this.#cachedEngine = null;
      }
    );

    return enginePromise;
  }

  /**
   * Create a TranslationsEngine and bypass the cache.
   *
   * @param {TranslationsChild} actor
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @returns {Promise<TranslationsEngine>}
   */
  static async create(actor, fromLanguage, toLanguage) {
    const startTime = actor.docShell.now();

    const engine = new TranslationsEngine(
      fromLanguage,
      toLanguage,
      await actor.getTranslationsEnginePayload(fromLanguage, toLanguage),
      actor.innerWindowId
    );

    await engine.isReady;

    ChromeUtils.addProfilerMarker(
      "TranslationsEngine",
      { innerWindowId: actor.innerWindowId, startTime },
      `Translations engine loaded for "${fromLanguage}" to "${toLanguage}"`
    );

    return engine;
  }

  /**
   * Only get the engine from the cache if it exists.
   */
  static getFromCache(fromLanguage, toLanguage) {
    if (
      this.#cachedEngine?.languagePairKey ===
      getLanguagePairKey(fromLanguage, toLanguage)
    ) {
      return this.#cachedEngine.enginePromise;
    }
    return null;
  }

  /**
   * @param {string} languagePairKey
   */
  static keepAlive(languagePairKey) {
    if (
      !TranslationsEngine.#cachedEngine?.languagePairKey !== languagePairKey
    ) {
      // It appears that the engine is already dead or another language pair
      // is being used.
      return;
    }
    if (TranslationsEngine.#keepAliveTimeout) {
      lazy.clearTimeout(TranslationsEngine.#keepAliveTimeout);
    }

    TranslationsEngine.#keepAliveTimeout = lazy.setTimeout(() => {
      // Terminate the engine worker.
      TranslationsEngine.#cachedEngine?.enginePromise.then(engine =>
        engine.terminate()
      );
    }, CACHE_TIMEOUT_MS);
  }

  /**
   * Load the translation engine and translate the page.
   *
   * @param {TranslationsChild} actor
   * @param {{fromLanguage: string, toLanguage: string}} langTags
   * @returns {Promise<void>}
   */
  static async translatePage(actor, { fromLanguage, toLanguage }) {
    const translationsStart = actor.docShell.now();

    const getEngine = () =>
      TranslationsEngine.getOrCreate(actor, fromLanguage, toLanguage);

    getEngine().catch(error => {
      actor.sendTelemetryError(error);
    });

    // Wait for the engine to be ready.
    const engine = await getEngine();

    const { document, innerWindowId } = actor;

    if (engine.translatedDoc?.innerWindowId === innerWindowId) {
      lazy.console.error("This page was already translated.");
      return;
    }

    const translatedDoc = new lazy.TranslationsDocument(
      document,
      fromLanguage,
      innerWindowId,
      html =>
        getEngine().then(engine =>
          engine.translateHTML([html], actor.innerWindowId)
        ),
      text =>
        getEngine().then(engine =>
          engine.translateText([text], actor.innerWindowId)
        ),
      () => this.docShell.now()
    );

    engine.translatedDoc = translatedDoc;

    lazy.console.log(
      "Beginning to translate.",
      actor.contentWindow.location.href
    );

    actor.sendEngineIsReady();

    translatedDoc.addRootElement(document.querySelector("title"));
    translatedDoc.addRootElement(
      document.body,
      true /* reportWordsInViewport */
    );

    translatedDoc.viewportTranslated.then(() => {
      ChromeUtils.addProfilerMarker(
        "TranslationsChild",
        { innerWindowId, startTime: actor.docShell.now() },
        "Viewport translations"
      );
      ChromeUtils.addProfilerMarker(
        "TranslationsChild",
        { innerWindowId, startTime: translationsStart },
        "Time to first translation"
      );
    });
  }

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
    this.languagePairKey = getLanguagePairKey(fromLanguage, toLanguage);
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
    TranslationsEngine.keepAlive(this.languagePairKey);

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
    TranslationsEngine.#cachedEngine?.then(engine => {
      if (engine === this) {
        TranslationsEngine.#cachedEngine = null;
      }
    });
  }

  /**
   * Stop processing the translation queue. All in-progress messages will be discarded.
   *
   * @param {number} innerWindowId
   */
  static discardTranslationQueue(innerWindowId) {
    TranslationsEngine.#cachedEngine?.enginePromise.then(engine => {
      ChromeUtils.addProfilerMarker(
        "TranslationsChild",
        null,
        "Request to discard translation queue"
      );
      engine.discardTranslationQueue(innerWindowId);
    });
  }

  /**
   * Stop processing the translation queue. All in-progress messages will be discarded.
   *
   * @param {number} innerWindowId
   */
  discardTranslationQueue(innerWindowId) {
    this.#translationsWorker.postMessage({
      type: "discard-translation-queue",
      innerWindowId,
    });
    this.translatedDoc = null;

    // Keep alive for another page laod.
    TranslationsEngine.keepAlive(this.languagePairKey);
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
