/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * @typedef {import("../content/translations-document.sys.mjs").TranslationsDocument} TranslationsDocument
 * @typedef {import("../translations").LanguageIdEngineMockedPayload} LanguageIdEngineMockedPayload
 * @typedef {import("../translations").LanguageIdEnginePayload} LanguageIdEnginePayload
 * @typedef {import("../translations").LanguageTranslationModelFiles} LanguageTranslationModelFiles
 * @typedef {import("../translations").TranslationsEnginePayload} TranslationsEnginePayload
 */

/**
 * @type {{
 *   TranslationsDocument: typeof TranslationsDocument
 *   console: typeof console
 * }}
 */
const lazy = {};

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

ChromeUtils.defineESModuleGetters(lazy, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  TranslationsDocument:
    "chrome://global/content/translations/translations-document.sys.mjs",
});

XPCOMUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.translations.logLevel",
    prefix: "Translations",
  });
});

export class LanguageIdEngine {
  /** @type {Worker} */
  #languageIdWorker;
  // Multiple messages can be sent before a response is received. This ID is used to keep
  // track of the messages. It is incremented on every use.
  #messageId = 0;

  /**
   * Construct and initialize the language-id worker.
   *
   * @param {Object} data
   * @property {string} data.type - The message type, expects "initialize".
   * @property {ArrayBuffer} data.wasmBuffer - The buffer containing the wasm binary.
   * @property {ArrayBuffer} data.modelBuffer - The buffer containing the language-id model binary.
   * @property {boolean} data.isLoggingEnabled
   */
  constructor(data) {
    this.#languageIdWorker = new Worker(
      "chrome://global/content/translations/language-id-engine-worker.js"
    );

    this.isReady = new Promise((resolve, reject) => {
      const onMessage = ({ data }) => {
        if (data.type === "initialization-success") {
          resolve();
        } else if (data.type === "initialization-failure") {
          reject(data.error);
        }
        this.#languageIdWorker.removeEventListener("message", onMessage);
      };
      this.#languageIdWorker.addEventListener("message", onMessage);
    });

    this.#languageIdWorker.postMessage(data);
  }

  /**
   * Attempts to identify the human language in which the message is written.
   * Generally, the longer a message is, the higher the likelihood that the
   * identified language will be correct. Shorter messages increase the chance
   * of false identification.
   *
   * The returned confidence is a number between 0.0 and 1.0 of how confident
   * the language identification model was that it identified the correct language.
   *
   * @param {string} message
   * @returns {Promise<{ languageLabel: string, confidence: number }>}
   */
  identifyLanguage(message) {
    const messageId = this.#messageId++;
    return new Promise((resolve, reject) => {
      const onMessage = ({ data }) => {
        if (data.messageId !== messageId) {
          // Multiple translation requests can be sent before a response is received.
          // Ensure that the response received here is the correct one.
          return;
        }
        if (data.type === "language-id-response") {
          let { languageLabel, confidence } = data;
          resolve({ languageLabel, confidence });
        }
        if (data.type === "language-id-error") {
          reject(data.error);
        }
        this.#languageIdWorker.removeEventListener("message", onMessage);
      };
      this.#languageIdWorker.addEventListener("message", onMessage);
      this.#languageIdWorker.postMessage({
        type: "language-id-request",
        message,
        messageId,
      });
    });
  }
}

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
      let enginePromise = this.#engines[fromLanguage + toLanguage];
      if (enginePromise) {
        return enginePromise;
      }
      if (onlyFromCache) {
        return null;
      }

      // A new engine needs to be created.
      enginePromise = actor.createTranslationsEngine(fromLanguage, toLanguage);

      this.#engines[fromLanguage + toLanguage] = enginePromise;

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
    const languagePair = fromLanguage + toLanguage;
    const timeoutId = this.#timeouts[languagePair];
    if (timeoutId) {
      lazy.clearTimeout(timeoutId);
    }
    const enginePromise = this.#engines[languagePair];
    if (!enginePromise) {
      // It appears that the engine is already dead.
      return;
    }
    this.#timeouts[languagePair] = lazy.setTimeout(() => {
      // Delete the caches.
      delete this.#engines[languagePair];
      delete this.#timeouts[languagePair];

      // Terminate the engine worker.
      enginePromise.then(engine => engine.terminate());
    }, CACHE_TIMEOUT_MS);
  }

  /**
   * Sees if an engine is still in the cache.
   */
  isInCache(fromLanguage, toLanguage) {
    this.keepAlive(fromLanguage, toLanguage);
    return Boolean(this.#engines[fromLanguage + toLanguage]);
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
   * @param {TranslationsEnginePayload} enginePayload
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
        } else if (data.type === "initialization-failure") {
          reject(data.error);
        }
        this.#translationsWorker.removeEventListener("message", onMessage);
      };
      this.#translationsWorker.addEventListener("message", onMessage);
    });

    this.#translationsWorker.postMessage({
      type: "initialize",
      fromLanguage,
      toLanguage,
      enginePayload,
      innerWindowId,
      messageId: this.#messageId++,
      logLevel: Services.prefs.getCharPref("browser.translations.logLevel"),
    });
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

/**
 * See the TranslationsParent for documentation.
 */
export class TranslationsChild extends JSWindowActorChild {
  constructor() {
    super();
    ChromeUtils.addProfilerMarker(
      "TranslationsChild",
      null,
      "TranslationsChild constructor"
    );
  }
  /**
   * The data for the Bergamot translation engine is downloaded from Remote Settings
   * and cached to disk. It is retained here in child process in case the translation
   * language switches.
   *
   * At the time of this writing ~5mb.
   *
   * @type {ArrayBuffer | null}
   */
  #bergamotWasmArrayBuffer = null;

  /**
   * The translations engine could be mocked for tests, since the wasm and the language
   * models must be downloaded from Remote Settings.
   */
  #isTranslationsEngineMocked = false;

  /**
   * The getter for the TranslationsEngine, managed by the EngineCache.
   *
   * @type {null | (() => Promise<TranslationsEngine>) | ((fromCache: true) => Promise<TranslationsEngine | null>)}
   */
  #getTranslationsEngine = null;

  /**
   * The actor can be destroyed leaving dangling references to dead objects.
   */
  #isDestroyed = false;

  /**
   * Store this at the beginning so that there is no risk of access a dead object
   * to read it.
   * @type {number | null}
   */
  innerWindowId = null;

  /**
   * @type {TranslationsDocument | null}
   */
  translatedDoc = null;

  /**
   * @returns {Promise<ArrayBuffer>}
   */
  async #getBergamotWasmArrayBuffer() {
    if (this.#isTranslationsEngineMocked) {
      throw new Error(
        "The engine is mocked, the Bergamot wasm is not available."
      );
    }
    let arrayBuffer = this.#bergamotWasmArrayBuffer;
    if (!arrayBuffer) {
      arrayBuffer = await this.sendQuery(
        "Translations:GetBergamotWasmArrayBuffer"
      );
      this.#bergamotWasmArrayBuffer = arrayBuffer;
    }
    return arrayBuffer;
  }

  /**
   * Retrieves the language-identification model binary from the TranslationsParent.
   *
   * @returns {Promise<ArrayBuffer>}
   */
  #getLanguageIdModelArrayBuffer() {
    return this.sendQuery("Translations:GetLanguageIdModelArrayBuffer");
  }

  /**
   * Retrieves the language-identification wasm binary from the TranslationsParent.
   *
   * @returns {Promise<ArrayBuffer>}
   */
  async #getLanguageIdWasmArrayBuffer() {
    return this.sendQuery("Translations:GetLanguageIdWasmArrayBuffer");
  }

  /**
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @returns {Promise<LanguageTranslationModelFiles[]>}
   */
  async #getLanguageTranslationModelFiles(fromLanguage, toLanguage) {
    if (this.#isTranslationsEngineMocked) {
      throw new Error(
        "The engine is mocked, there are no language model files available."
      );
    }
    return this.sendQuery("Translations:GetLanguageTranslationModelFiles", {
      fromLanguage,
      toLanguage,
    });
  }

  /**
   * @param {{ type: string }} event
   */
  handleEvent(event) {
    ChromeUtils.addProfilerMarker(
      "TranslationsChild",
      null,
      "Event: " + event.type
    );
    if (event.type === "DOMContentLoaded") {
      this.innerWindowId = this.contentWindow.windowGlobalChild.innerWindowId;
      this.maybeTranslateDocument();
    }
  }

  async maybeTranslateDocument() {
    const { href } = this.contentWindow.location;
    if (
      !href.startsWith("http://") &&
      !href.startsWith("https://") &&
      !href.startsWith("file:///")
    ) {
      return;
    }

    const translationsStart = this.docShell.now();
    let appLangTag = new Intl.Locale(Services.locale.appLocaleAsBCP47).language;
    let docLangTag;

    try {
      const docLocale = new Intl.Locale(this.document.documentElement.lang);
      docLangTag = docLocale.language;
    } catch (error) {}

    if (!docLangTag) {
      const message = "No valid language detected.";
      ChromeUtils.addProfilerMarker(
        "TranslationsChild",
        { innerWindowId: this.innerWindowId },
        message
      );
      lazy.console.log(message, this.contentWindow.location.href);
      return;
    }

    if (appLangTag === docLangTag) {
      const message =
        "The app and document languages match, so not translating.";
      ChromeUtils.addProfilerMarker(
        "TranslationsChild",
        { innerWindowId: this.innerWindowId },
        message
      );
      lazy.console.log(message, this.contentWindow.location.href);
      return;
    }

    // There is no reason to look at supported languages if the engine is already in
    // the cache.
    if (!translationsEngineCache.isInCache(docLangTag, appLangTag)) {
      // TODO - This is wrong for non-bidirectional translation pairs.
      const supportedLanguages = await this.getSupportedLanguages();
      if (this.#isDestroyed) {
        return;
      }
      if (
        !supportedLanguages.some(({ langTag }) => langTag === appLangTag) ||
        !supportedLanguages.some(({ langTag }) => langTag === docLangTag)
      ) {
        const message = `Translating from "${docLangTag}" to "${appLangTag}" is not supported.`;
        ChromeUtils.addProfilerMarker(
          "TranslationsChild",
          { innerWindowId: this.innerWindowId },
          message
        );
        lazy.console.log(message, supportedLanguages);
        return;
      }
    }

    try {
      const engineLoadStart = this.docShell.now();

      // Create a function to get an engine. These engines are pretty heavy in terms
      // of memory usage, so they will be destroyed when not in use, and attempt to
      // be re-used when loading a new page.
      this.#getTranslationsEngine = await translationsEngineCache.createGetter(
        this,
        docLangTag,
        appLangTag
      );
      if (this.#isDestroyed) {
        return;
      }

      // Start loading the engine if it doesn't exist.
      this.#getTranslationsEngine().then(
        () => {
          ChromeUtils.addProfilerMarker(
            "TranslationsChild",
            { innerWindowId: this.innerWindowId, startTime: engineLoadStart },
            "Load Translations Engine"
          );
        },
        error => {
          lazy.console.error("Failed to load the translations engine.", error);
        }
      );
    } catch (error) {
      lazy.console.error(
        "Failed to load the translations engine",
        error,
        this.contentWindow.location.href
      );
      return;
    }

    this.translatedDoc = new lazy.TranslationsDocument(
      this.document,
      docLangTag,
      this.innerWindowId,
      html =>
        this.#getTranslationsEngine().then(engine =>
          engine.translateHTML([html], this.innerWindowId)
        ),
      text =>
        this.#getTranslationsEngine().then(engine =>
          engine.translateText([text], this.innerWindowId)
        ),
      () => this.docShell.now()
    );

    lazy.console.log(
      "Beginning to translate.",
      this.contentWindow.location.href
    );

    this.translatedDoc.addRootElement(this.document.querySelector("title"));
    this.translatedDoc.addRootElement(
      this.document.body,
      true /* reportWordsInViewport */
    );

    {
      const startTime = this.docShell.now();
      this.translatedDoc.viewportTranslated.then(() => {
        ChromeUtils.addProfilerMarker(
          "TranslationsChild",
          { innerWindowId: this.innerWindowId, startTime },
          "Viewport translations"
        );
        ChromeUtils.addProfilerMarker(
          "TranslationsChild",
          { innerWindowId: this.innerWindowId, startTime: translationsStart },
          "Time to first translation"
        );
      });
    }
  }

  /**
   * Receive a message from the parent.
   *
   * @param {{ name: string, data: any }} message
   */
  receiveMessage(message) {
    switch (message.name) {
      case "Translations:IsMocked":
        this.#isTranslationsEngineMocked = message.data;
        break;
      default:
        lazy.console.warn("Unknown message.");
    }
  }

  /**
   * Get the list of languages and their display names, sorted by their display names.
   *
   * TODO (Bug 1813775) - Not all languages have bi-directional translations, like
   * Icelandic. These are listed as "Beta" in the addon. This list should be changed into
   * a "from" and "to" list, and the logic enhanced in the dropdowns to only allow valid
   * translations.
   *
   * @returns {Promise<Array<{ langTag: string, displayName }>>}
   */
  getSupportedLanguages() {
    return this.sendQuery("Translations:GetSupportedLanguages");
  }

  /**
   * Retrieve the payload for creating a LanguageIdEngine.
   *
   * @returns {Promise<LanguageIdEnginePayload | LanguageIdEngineMockedPayload>}
   */
  async #getLanguageIdEnginePayload() {
    // If the TranslationsParent has a mocked payload defined for testing purposes,
    // then we will return the mocked payload, otherwise we will attempt to retrieve
    // the full payload from Remote Settings.
    const mockedPayload = await this.sendQuery(
      "Translations:GetLanguageIdEngineMockedPayload"
    );
    if (mockedPayload) {
      const { languageLabel, confidence } = mockedPayload;
      return {
        languageLabel,
        confidence,
      };
    }

    const [wasmBuffer, modelBuffer] = await Promise.all([
      this.#getLanguageIdWasmArrayBuffer(),
      this.#getLanguageIdModelArrayBuffer(),
    ]);
    return {
      modelBuffer,
      wasmBuffer,
    };
  }

  /**
   * The engine is not available in tests.
   *
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @returns {null | TranslationsEnginePayload}
   */
  async #getTranslationsEnginePayload(fromLanguage, toLanguage) {
    if (this.#isTranslationsEngineMocked) {
      return null;
    }
    const [bergamotWasmArrayBuffer, languageModelFiles] = await Promise.all([
      this.#getBergamotWasmArrayBuffer(),
      this.#getLanguageTranslationModelFiles(fromLanguage, toLanguage),
    ]);
    return { bergamotWasmArrayBuffer, languageModelFiles };
  }

  /**
   * Construct and initialize the LanguageIdEngine.
   *
   * @returns {LanguageIdEngine}
   */
  async createLanguageIdEngine() {
    const {
      confidence,
      languageLabel,
      modelBuffer,
      wasmBuffer,
    } = await this.#getLanguageIdEnginePayload();
    const engine = new LanguageIdEngine({
      type: "initialize",
      confidence,
      languageLabel,
      modelBuffer,
      wasmBuffer,
      isLoggingEnabled:
        Services.prefs.getCharPref("browser.translations.logLevel") === "All",
    });
    await engine.isReady;
    return engine;
  }

  /**
   * Construct and initialize the Translations Engine.
   *
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @returns {TranslationsEngine | null}
   */
  async createTranslationsEngine(fromLanguage, toLanguage) {
    const startTime = this.docShell.now();

    const enginePayload = await this.#getTranslationsEnginePayload(
      fromLanguage,
      toLanguage
    );

    const engine = new TranslationsEngine(
      fromLanguage,
      toLanguage,
      enginePayload,
      this.innerWindowId
    );

    await engine.isReady;

    ChromeUtils.addProfilerMarker(
      "TranslationsChild",
      { innerWindowId: this.innerWindowId, startTime },
      `Translations engine loaded for "${fromLanguage}" to "${toLanguage}"`
    );
    return engine;
  }

  /**
   * Override JSWindowActorChild.prototype.didDestroy. This is called by the actor
   * manager when the actor was destroyed.
   */
  async didDestroy() {
    this.#isDestroyed = true;
    const getTranslationsEngine = this.#getTranslationsEngine;
    if (!getTranslationsEngine) {
      return;
    }
    const engine = await getTranslationsEngine(
      // Just get it from cache, don't create a new one.
      true
    );
    if (engine) {
      // Discard the queue otherwise the worker will continue to translate.
      engine.discardTranslationQueue(this.innerWindowId);

      // Keep it alive long enough for another page load.
      translationsEngineCache.keepAlive(engine.fromLanguage, engine.toLanguage);
    }
  }
}
