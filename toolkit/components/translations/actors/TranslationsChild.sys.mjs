/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

XPCOMUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.translations.logLevel",
    prefix: "Translations",
  });
});

/**
 * @typedef {import("../translations").LanguageTranslationModelFiles} LanguageTranslationModelFiles
 * @typedef {import("../translations").TranslationsEnginePayload} TranslationsEnginePayload
 */

export class LanguageIdEngine {
  /** @type {Worker} */
  #languageIdWorker;
  // Multiple messages can be sent before a response is received. This ID is used to keep
  // track of the messages. It is incremented on every use.
  #messageId = 0;

  /**
   * Construct and initialize the language-id worker.
   *
   * @param {ArrayBuffer} wasmArrayBuffer
   * @param {ArrayBuffer} languageIdModel
   */
  constructor(wasmBuffer, modelBuffer) {
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

    this.#languageIdWorker.postMessage({
      type: "initialize",
      wasmBuffer,
      modelBuffer,
      isLoggingEnabled:
        Services.prefs.getCharPref("browser.translations.logLevel") === "All",
    });
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
   */
  constructor(fromLanguage, toLanguage, enginePayload) {
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
      messageId: this.#messageId++,
      isLoggingEnabled:
        Services.prefs.getCharPref("browser.translations.logLevel") === "All",
    });
  }

  /**
   * Translate text without any HTML.
   *
   * @param {string[]} messageBatch
   * @returns {Promise<string[]>}
   */
  translateText(messageBatch) {
    return this.#translate(messageBatch, false);
  }

  /**
   * Translate valid HTML. Note that this method throws if invalid markup is provided.
   *
   * @param {string[]} messageBatch
   * @returns {Promise<string[]>}
   */
  translateHTML(messageBatch) {
    return this.#translate(messageBatch, true);
  }

  /**
   * The implementation for translation. Use translateText or translateHTML for the
   * public API.
   *
   * @param {string[]} messageBatch
   * @param {boolean} isHTML
   * @returns {Promise<string[]>}
   */
  #translate(messageBatch, isHTML) {
    const messageId = this.#messageId++;
    lazy.console.log("Translating", messageBatch);

    return new Promise((resolve, reject) => {
      const onMessage = ({ data }) => {
        lazy.console.log("Received message", data);
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
}

/**
 * See the TranslationsParent for documentation.
 */
export class TranslationsChild extends JSWindowActorChild {
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
    // TODO
    lazy.console.log("TranslationsChild observed a pageshow event.");
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
   * Construct and initialize the LanguageId Engine.
   */
  async createLanguageIdEngine() {
    const [wasmBuffer, languageIdModelArrayBuffer] = await Promise.all([
      this.#getLanguageIdWasmArrayBuffer(),
      this.#getLanguageIdModelArrayBuffer(),
    ]);
    const engine = new LanguageIdEngine(wasmBuffer, languageIdModelArrayBuffer);
    await engine.isReady;
    return engine;
  }

  /**
   * The engine is not avialable in tests.
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
   * Construct and initialize the Translations Engine.
   *
   * @param {string} fromLanguage
   * @param {string} toLanguage
   */
  async createTranslationsEngine(fromLanguage, toLanguage) {
    const enginePayload = await this.#getTranslationsEnginePayload(
      fromLanguage,
      toLanguage
    );

    const engine = new TranslationsEngine(
      fromLanguage,
      toLanguage,
      enginePayload
    );

    await engine.isReady;
    return engine;
  }
}
