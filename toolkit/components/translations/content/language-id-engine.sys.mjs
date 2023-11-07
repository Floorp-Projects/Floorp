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

ChromeUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.translations.logLevel",
    prefix: "Translations",
  });
});

ChromeUtils.defineESModuleGetters(lazy, {
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
  clearTimeout: "resource://gre/modules/Timer.sys.mjs",
});

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
 *
 * For the moment, while we investigate which language identification library
 * we would like to use, keep this logic in sync with LanguageDetector.sys.mjs
 */
const DOC_TEXT_TO_IDENTIFY_LENGTH = 1024;

export class LanguageIdEngine {
  /** @type {Worker} */
  #languageIdWorker;
  // Multiple messages can be sent before a response is received. This ID is used to keep
  // track of the messages. It is incremented on every use.
  #messageId = 0;

  static #cachedEngine = null;
  static #cachedEngineTimeoutId = null;
  static #cachedEngineTimeoutMS = 30_000;

  /**
   * Gets a cached engine, or creates a new one. Returns `null` when the engine
   * payload fails to download.
   *
   * @param {() => Object} getPayload
   * @returns {LanguageIdEngine | null}
   */
  static getOrCreate(getPayload) {
    if (!this.#cachedEngine) {
      this.#cachedEngine = LanguageIdEngine.#create(getPayload);
    }
    return this.#cachedEngine;
  }

  /**
   * @param {() => Object} getPayload
   * @returns {Promise<LanguageIdEngine | null>}
   */
  static async #create(getPayload) {
    let payload;
    try {
      payload = await getPayload();
    } catch (error) {
      // The payload may not be able to be downloaded. Report this as a normal
      // console.log, as this is the default behavior in automation.
      lazy.console.log(
        "The language id payload was unable to be downloaded.",
        error
      );
      return null;
    }

    const engine = new LanguageIdEngine(payload);
    await engine.isReady;
    LanguageIdEngine.#resetCacheTimeout();
    return engine;
  }

  static #resetCacheTimeout() {
    if (LanguageIdEngine.#cachedEngineTimeoutId) {
      lazy.clearTimeout(LanguageIdEngine.#cachedEngineTimeoutId);
    }
    LanguageIdEngine.#cachedEngineTimeoutId = lazy.setTimeout(
      LanguageIdEngine.#clearEngineCache,
      LanguageIdEngine.#cachedEngineTimeoutMS
    );
  }

  static #clearEngineCache() {
    lazy.console.log("Clearing the engine cache");
    LanguageIdEngine.#cachedEngine = null;
    LanguageIdEngine.#cachedEngineTimeoutId = null;
  }

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

    this.#languageIdWorker.postMessage(
      {
        type: "initialize",
        isLoggingEnabled: lazy.logLevel === "All",
        ...data,
      },
      transferables
    );
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
    LanguageIdEngine.#resetCacheTimeout();
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

  /**
   * Attempts to determine the language in which the document's content is written.
   *
   * For the moment, while we investigate which language identification library
   * we would like to use, keep this logic in sync with LanguageDetector.sys.mjs
   * @returns {string | null}
   */
  async identifyLanguageFromDocument(document) {
    // Grab a selection of text.
    let encoder = Cu.createDocumentEncoder("text/plain");
    encoder.init(document, "text/plain", encoder.SkipInvisibleContent);
    let text = encoder
      .encodeToStringWithMaxLength(DOC_TEXT_TO_IDENTIFY_LENGTH)
      .replaceAll("\r", "")
      .replaceAll("\n", " ");

    let { langTag, confidence } = await this.identifyLanguage(text);

    lazy.console.log(
      `${langTag}(${confidence.toFixed(2)}) Detected Page Language`
    );
    return confidence >= DOC_LANGUAGE_DETECTION_THRESHOLD ? langTag : null;
  }
}
