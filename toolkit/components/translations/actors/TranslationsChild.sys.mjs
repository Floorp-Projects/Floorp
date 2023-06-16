/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * @typedef {import("../content/translations-document.sys.mjs").TranslationsDocument} TranslationsDocument
 * @typedef {import("../translations").LanguageIdEnginePayload} LanguageIdEnginePayload
 * @typedef {import("../translations").LanguageTranslationModelFiles} LanguageTranslationModelFiles
 * @typedef {import("../translations").TranslationsEnginePayload} TranslationsEnginePayload
 * @typedef {import("../translations").LanguagePair} LanguagePair
 * @typedef {import("../translations").SupportedLanguages} SupportedLanguages
 * @typedef {import("../translations").LangTags} LangTags
 */

/**
 * @type {{
 *   TranslationsDocument: typeof TranslationsDocument
 *   console: typeof console
 * }}
 */
const lazy = {};

/**
 * The threshold that the language-identification confidence
 * value must be greater than in order to provide the detected language
 * tag for translations.
 *
 * This value should ideally be one that does not allow false positives
 * while also not being too restrictive.
 *
 * At this time, this value is not driven by statistical data or analysis.
 */
const DOC_LANGUAGE_DETECTION_THRESHOLD = 0.65;

/**
 * The length of the substring to pull from the document's text for language
 * identification.
 *
 * This value should ideally be one that is large enough to yield a confident
 * identification result without being too large or expensive to extract.
 *
 * At this time, this value is not driven by statistical data or analysis.
 */
const DOC_TEXT_TO_IDENTIFY_LENGTH = 1024;

const PIVOT_LANGUAGE = "en";

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

ChromeUtils.defineESModuleGetters(lazy, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
  TranslationsDocument:
    "chrome://global/content/translations/translations-document.sys.mjs",
  TranslationsTelemetry:
    "chrome://global/content/translations/TranslationsTelemetry.sys.mjs",
});

XPCOMUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.translations.logLevel",
    prefix: "Translations",
  });
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "logLevel",
  "browser.translations.logLevel"
);

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
   * @param {string} data.type - The message type, expects "initialize".
   * @param {ArrayBuffer} data.wasmBuffer - The buffer containing the wasm binary.
   * @param {ArrayBuffer} data.modelBuffer - The buffer containing the language-id model binary.
   * @param {null | string} data.mockedLangTag - The mocked language tag value (only present when mocking).
   * @param {null | number} data.mockedConfidence - The mocked confidence value (only present when mocking).
   * @param {boolean} data.isLoggingEnabled
   */
  constructor(data) {
    this.#languageIdWorker = new Worker(
      "chrome://global/content/translations/language-id-engine-worker.js"
    );

    this.isReady = new Promise((resolve, reject) => {
      const onMessage = ({ data }) => {
        if (data.type === "initialization-success") {
          resolve();
        } else if (data.type === "initialization-error") {
          reject(data.error);
        }
        this.#languageIdWorker.removeEventListener("message", onMessage);
      };
      this.#languageIdWorker.addEventListener("message", onMessage);
    });

    const transferables = [];
    // Make sure the ArrayBuffers are transferred, not cloned.
    // https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API/Transferable_objects
    transferables.push(data.wasmBuffer, data.modelBuffer);

    this.#languageIdWorker.postMessage(data, transferables);
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
   * @returns {Promise<{ langTag: string, confidence: number }>}
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
          let { langTag, confidence } = data;
          resolve({ langTag, confidence });
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
   * The matched language tags for the page. Used to find a default language pair for
   * translations.
   *
   * @type {null | LangTags}
   * */
  #langTags = null;

  /**
   * Creates a lookup key that is unique to each fromLanguage-toLanguage pair.
   *
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @returns {string}
   */
  static languagePairKey(fromLanguage, toLanguage) {
    return `${fromLanguage},${toLanguage}`;
  }

  /**
   * Retrieve a substring of text from the document body to be
   * analyzed by the LanguageIdEngine to determine the page's language.
   *
   * @returns {string}
   */
  #getTextToIdentify() {
    let encoder = Cu.createDocumentEncoder("text/plain");
    encoder.init(this.document, "text/plain", encoder.SkipInvisibleContent);
    return encoder
      .encodeToStringWithMaxLength(DOC_TEXT_TO_IDENTIFY_LENGTH)
      .replaceAll("\r", "")
      .replaceAll("\n", " ");
  }

  /**
   * @overrides JSWindowActorChild.prototype.handleEvent
   * @param {{ type: string }} event
   */
  handleEvent(event) {
    ChromeUtils.addProfilerMarker(
      "TranslationsChild",
      null,
      "Event: " + event.type
    );
    switch (event.type) {
      case "DOMContentLoaded":
        this.innerWindowId = this.contentWindow.windowGlobalChild.innerWindowId;
        this.maybeOfferTranslation().catch(error => lazy.console.log(error));
        break;
      case "pagehide":
        lazy.console.log(
          "pagehide",
          this.contentWindow.location,
          this.#langTags
        );
        this.reportDetectedLangTagsToParent(null);
        break;
    }
    return undefined;
  }

  /**
   * This is used to conditionally add the translations button.
   * @param {null | LangTags} langTags
   */
  reportDetectedLangTagsToParent(langTags) {
    this.sendAsyncMessage("Translations:ReportDetectedLangTags", {
      langTags,
    });
  }

  /**
   * Returns the principal from the content window's origin.
   * @returns {nsIPrincipal}
   */
  getContentWindowPrincipal() {
    return Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      this.contentWindow.location.origin
    );
  }

  /**
   * Only translate pages that match certain protocols, that way internal pages like
   * about:* pages will not be translated.
   */
  #isRestrictedPage() {
    const { href } = this.contentWindow.location;
    // Keep this logic up to date with TranslationsParent.isRestrictedPage.
    return !(
      href.startsWith("http://") ||
      href.startsWith("https://") ||
      href.startsWith("file:///")
    );
  }

  /**
   * Determine if the page should be translated by checking the App's languages and
   * comparing it to the reported language of the page. Return the best translation fit
   * (if available).
   *
   * @param {number} [translationsStart]
   * @returns {Promise<LangTags>}
   */
  async getLangTagsForTranslation(translationsStart = this.docShell.now()) {
    if (this.#langTags) {
      return this.#langTags;
    }

    const langTags = {
      docLangTag: null,
      userLangTag: null,
      isDocLangTagSupported: false,
    };
    this.#langTags = langTags;

    if (this.#isRestrictedPage()) {
      // The langTags are still blank here.
      return langTags;
    }
    let languagePairs = await this.getLanguagePairs();

    const determineIsDocLangTagSupported = docLangTag =>
      Boolean(
        languagePairs.find(({ fromLang }) => fromLang === langTags.docLangTag)
      );

    // First try to get the langTag from the document's markup.
    try {
      const docLocale = new Intl.Locale(this.document.documentElement.lang);
      langTags.docLangTag = docLocale.language;
      langTags.isDocLangTagSupported = determineIsDocLangTagSupported(
        docLocale.language
      );
    } catch (error) {}

    // If the document's markup had no specified langTag, attempt
    // to identify the page's language using the LanguageIdEngine.
    if (!langTags.docLangTag) {
      let languageIdEngine = await this.createLanguageIdEngine();
      let { langTag, confidence } = await languageIdEngine.identifyLanguage(
        this.#getTextToIdentify()
      );
      lazy.console.log(
        `${langTag}(${confidence.toFixed(2)}) Detected Page Language`
      );
      if (confidence >= DOC_LANGUAGE_DETECTION_THRESHOLD) {
        langTags.docLangTag = langTag;
        langTags.isDocLangTagSupported =
          determineIsDocLangTagSupported(langTag);
      }
    }

    const preferredLanguages = await this.getPreferredLanguages();

    if (!langTags.docLangTag) {
      const message = "No valid language detected.";
      ChromeUtils.addProfilerMarker(
        "TranslationsChild",
        { innerWindowId: this.innerWindowId },
        message
      );
      lazy.console.log(message, this.contentWindow.location.href);

      const languagePairs = await this.getLanguagePairs();

      // Attempt to find a good language to select for the user.
      langTags.userLangTag =
        preferredLanguages.find(langTag => langTag === languagePairs.toLang) ??
        null;

      return langTags;
    }

    ChromeUtils.addProfilerMarker(
      "TranslationsChild",
      { innerWindowId: this.innerWindowId, startTime: translationsStart },
      "Time to determine langTags"
    );

    // This is a special case where we do not offer a translation if the main app language
    // and the doc language match. The main app language should be the first preferred
    // language.
    if (preferredLanguages[0] === langTags.docLangTag) {
      // The doc language and the main language match.
      const message =
        "The app and document languages match, so not translating.";
      ChromeUtils.addProfilerMarker(
        "TranslationsChild",
        { innerWindowId: this.innerWindowId },
        message
      );
      lazy.console.log(message, this.contentWindow.location.href);
      // The docLangTag will be set, while the userLangTag will be null.
      return langTags;
    }

    // Attempt to find a matching language pair for a preferred language.
    for (const preferredLangTag of preferredLanguages) {
      if (
        translationsEngineCache.isInCache(langTags.docLangTag, preferredLangTag)
      ) {
        // There is no reason to look at the language pairs if the engine is already in
        // the cache.
        langTags.userLangTag = preferredLangTag;
        break;
      }

      if (!langTags.isDocLangTagSupported) {
        if (languagePairs.some(({ toLang }) => toLang === preferredLangTag)) {
          // Only match the "to" language, since the "from" is not supported.
          langTags.userLangTag = preferredLangTag;
        }
        break;
      }

      // Is there a direct language pair match?
      if (
        languagePairs.some(
          ({ fromLang, toLang }) =>
            fromLang === langTags.docLangTag && toLang === preferredLangTag
        )
      ) {
        // A match was found in one of the preferred languages.
        langTags.userLangTag = preferredLangTag;
        break;
      }

      // Is there a pivot language match?
      if (
        // Match doc -> pivot
        languagePairs.some(
          ({ fromLang, toLang }) =>
            fromLang === langTags.docLangTag && toLang === PIVOT_LANGUAGE
        ) &&
        // Match pivot -> preferred language
        languagePairs.some(
          ({ fromLang, toLang }) =>
            fromLang === PIVOT_LANGUAGE && toLang === preferredLangTag
        )
      ) {
        langTags.userLangTag = preferredLangTag;
        break;
      }
    }

    if (!langTags.userLangTag) {
      // No language pairs match.
      const message = `No matching translation pairs were found for translating from "${langTags.docLangTag}".`;
      ChromeUtils.addProfilerMarker(
        "TranslationsChild",
        { innerWindowId: this.innerWindowId },
        message
      );
      lazy.console.log(message, languagePairs);
    }

    return langTags;
  }

  /**
   * Deduce the language tags on the page, and either:
   *  1. Show an offer to translate.
   *  2. Auto-translate.
   *  3. Do nothing.
   */
  async maybeOfferTranslation() {
    const translationsStart = this.docShell.now();

    const isSupported = await this.isTranslationsEngineSupported;
    if (!isSupported) {
      return;
    }

    const langTags = await this.getLangTagsForTranslation(translationsStart);

    this.#langTags = langTags;
    this.reportDetectedLangTagsToParent(langTags);

    if (langTags.docLangTag && langTags.userLangTag) {
      const { maybeAutoTranslate, maybeNeverTranslate } = await this.sendQuery(
        "Translations:GetTranslationConditions",
        langTags
      );
      if (maybeAutoTranslate && !maybeNeverTranslate) {
        lazy.TranslationsTelemetry.onTranslate({
          fromLanguage: langTags.docLangTag,
          toLanguage: langTags.userLangTag,
          autoTranslate: maybeAutoTranslate,
        });
        this.translatePage(
          langTags.docLangTag,
          langTags.userLangTag,
          translationsStart
        );
      }
    }
  }

  /**
   * Lazily initialize this value. It doesn't change after being set.
   *
   * @type {Promise<boolean>}
   */
  get isTranslationsEngineSupported() {
    // Delete the getter and set the real value directly onto the TranslationsChild's
    // prototype. This value never changes while a browser is open.
    delete TranslationsChild.isTranslationsEngineSupported;
    return (TranslationsChild.isTranslationsEngineSupported = this.sendQuery(
      "Translations:GetIsTranslationsEngineSupported"
    ));
  }

  /**
   * Load the translation engine and translate the page.
   *
   * @param {{fromLanguage: string, toLanguage: string}} langTags
   * @param {number} [translationsStart]
   * @returns {Promise<void>}
   */
  async translatePage(
    fromLanguage,
    toLanguage,
    translationsStart = this.docShell.now()
  ) {
    if (this.translatedDoc) {
      lazy.console.warn("This page was already translated.");
      return;
    }
    if (this.#isRestrictedPage()) {
      lazy.console.warn("Attempting to translate a restricted page.");
      return;
    }

    try {
      const engineLoadStart = this.docShell.now();
      // Create a function to get an engine. These engines are pretty heavy in terms
      // of memory usage, so they will be destroyed when not in use, and attempt to
      // be re-used when loading a new page.
      this.#getTranslationsEngine = await translationsEngineCache.createGetter(
        this,
        fromLanguage,
        toLanguage
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
          lazy.console.log("Failed to load the translations engine.", error);
        }
      );
    } catch (error) {
      lazy.TranslationsTelemetry.onError(error);
      lazy.console.log(
        "Failed to load the translations engine",
        error,
        this.contentWindow.location.href
      );
      this.sendAsyncMessage("Translations:FullPageTranslationFailed", {
        reason: "engine-load-failure",
      });
      return;
    }

    // Ensure the translation engine loads correctly at least once before instantiating
    // the TranslationsDocument.
    try {
      await this.#getTranslationsEngine();
    } catch (error) {
      lazy.TranslationsTelemetry.onError(error);
      this.sendAsyncMessage("Translations:FullPageTranslationFailed", {
        reason: "engine-load-failure",
      });
      return;
    }

    this.translatedDoc = new lazy.TranslationsDocument(
      this.document,
      fromLanguage,
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

    this.sendAsyncMessage("Translations:EngineIsReady");

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
  receiveMessage({ name, data }) {
    switch (name) {
      case "Translations:TranslatePage":
        const langTags = data ?? this.#langTags;
        if (!langTags) {
          lazy.console.warn(
            "Attempting to translate a page, but no language tags were given."
          );
          break;
        }
        lazy.TranslationsTelemetry.onTranslate({
          fromLanguage: langTags.fromLanguage,
          toLanguage: langTags.toLanguage,
          autoTranslate: false,
        });
        this.translatePage(langTags.fromLanguage, langTags.toLanguage);
        break;
      case "Translations:GetLangTagsForTranslation":
        return this.getLangTagsForTranslation();
      case "Translations:GetContentWindowPrincipal":
        return this.getContentWindowPrincipal();
      default:
        lazy.console.warn("Unknown message.", name);
    }
    return undefined;
  }

  /**
   * Get the list of languages and their display names, sorted by their display names.
   * This is more expensive of a call than getLanguagePairs since the display names
   * are looked up.
   *
   * @returns {Promise<Array<SupportedLanguages>>}
   */
  getSupportedLanguages() {
    return this.sendQuery("Translations:GetSupportedLanguages");
  }

  /**
   * @param {string} language The BCP 47 language tag.
   */
  hasAllFilesForLanguage(language) {
    return this.sendQuery("Translations:HasAllFilesForLanguage", {
      language,
    });
  }

  /**
   * @param {string} language The BCP 47 language tag.
   */
  deleteLanguageFiles(language) {
    return this.sendQuery("Translations:DeleteLanguageFiles", {
      language,
    });
  }

  /**
   * @param {string} language The BCP 47 language tag.
   */
  downloadLanguageFiles(language) {
    return this.sendQuery("Translations:DownloadLanguageFiles", {
      language,
    });
  }

  /**
   * Download all files from Remote Settings.
   */
  downloadAllFiles() {
    return this.sendQuery("Translations:DownloadAllFiles");
  }

  /**
   * Delete all language files.
   * @returns {Promise<string[]>} Returns a list of deleted record ids.
   */
  deleteAllLanguageFiles() {
    return this.sendQuery("Translations:DeleteAllLanguageFiles");
  }

  /**
   * Get the language pairs that can be used for translations. This is cheaper than
   * the getSupportedLanguages call, since the localized display names of the languages
   * are not needed.
   *
   * @returns {Promise<Array<LanguagePair>>}
   */
  getLanguagePairs() {
    return this.sendQuery("Translations:GetLanguagePairs");
  }

  /**
   * The ordered list of preferred BCP 47 language tags.
   *
   *   1. App languages
   *   2. Web requested languages
   *   3. OS languages
   *
   * @returns {Promise<string[]>}
   */
  getPreferredLanguages() {
    return this.sendQuery("Translations:GetPreferredLanguages");
  }

  /**
   * Retrieve the payload for creating a LanguageIdEngine.
   *
   * @returns {Promise<LanguageIdEnginePayload>}
   */
  async #getLanguageIdEnginePayload() {
    return this.sendQuery("Translations:GetLanguageIdEnginePayload");
  }

  /**
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @returns {TranslationsEnginePayload}
   */
  async #getTranslationsEnginePayload(fromLanguage, toLanguage) {
    return this.sendQuery("Translations:GetTranslationsEnginePayload", {
      fromLanguage,
      toLanguage,
    });
  }

  /**
   * Construct and initialize the LanguageIdEngine.
   *
   * @returns {LanguageIdEngine}
   */
  async createLanguageIdEngine() {
    const payload = await this.#getLanguageIdEnginePayload();
    const engine = new LanguageIdEngine({
      type: "initialize",
      isLoggingEnabled: lazy.logLevel === "All",
      ...payload,
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
