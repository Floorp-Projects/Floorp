/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/chrome-worker */
"use strict";

// Throw Promise rejection errors so that they are visible in the console.
self.addEventListener("unhandledrejection", event => {
  throw event.reason;
});

/* global addOnPostRun FastText loadFastText */
importScripts(
  "chrome://global/content/translations/fasttext.js",
  "chrome://global/content/translations/fasttext_wasm.js"
);

/**
 * The number of languages that should be returned when the model analyzes text.
 *
 * A value of 1 means only the most-likely language will be returned.
 * A value of 5 would mean that the top 5 most-likely languages will be returned.
 */
const LANGUAGE_COUNT = 1;

/**
 * The threshold of likelihood in range [0.0, 1.0] that must pass
 * for a language to be returned from the model.
 *
 * A value of 0.0 would mean that a language is always returned with any confidence.
 * A value of 0.5 would mean that a language is only returned if the model
 * is 50% confident that the analyzed text could be that language.
 */
const CONFIDENCE_THRESHOLD = 0.0;

// Respect the preference "browser.translations.logLevel".
let _isLoggingEnabled = true;
function log(...args) {
  if (_isLoggingEnabled) {
    console.log("Translations:", ...args);
  }
}

// Wait for the initialization request.
addEventListener("message", initialize);

/**
 * Initialize the engine, and get it ready to handle language identification requests.
 * The "initialize" message must be received before any other message handling
 * requests will be processed.
 *
 * @param {Object} data
 * @property {string} data.type - The message type, expects "initialize".
 * @property {ArrayBuffer} data.wasmBuffer - The buffer containing the wasm binary.
 * @property {ArrayBuffer} data.modelBuffer - The buffer containing the language-id model binary.
 * @property {boolean} data.isLoggingEnabled
 */
async function initialize({ data }) {
  if (data.type !== "initialize") {
    throw new Error(
      "The LanguageIdEngine worker received a message before it was initialized."
    );
  }

  try {
    const { modelBuffer, wasmBuffer, isLoggingEnabled } = data;

    if (!modelBuffer) {
      throw new Error('Worker initialization missing "modelBuffer"');
    }
    if (!wasmBuffer) {
      throw new Error('Worker initialization missing "wasmBuffer"');
    }
    if (isLoggingEnabled) {
      // Respect the "browser.translations.logLevel" preference.
      _isLoggingEnabled = true;
    }

    let promise = new Promise((resolve, reject) => {
      const initialModule = {
        onAbort() {
          reject(new Error("Error loading the fastText Wasm Module"));
        },
        onRuntimeInitialized() {
          addOnPostRun(() => {
            const ft = new FastText(initialModule);
            const model = ft.loadModelBinary(modelBuffer);
            resolve(model);
          });
        },
        wasmBinary: wasmBuffer,
      };
      loadFastText(initialModule);
    });

    let model = await promise;
    new LanguageIdWorker(model);
    postMessage({ type: "initialization-success" });
  } catch (error) {
    console.error(error);
    postMessage({ type: "initialization-error", error: error?.message });
  }

  removeEventListener("message", initialize);
}

/**
 * The LanguageIdWorker listens for messages that contain text for which to identify
 * the human language in which the text is written.
 */
class LanguageIdWorker {
  /**
   * @param {FastTextModel} model
   */
  constructor(model) {
    /** @type {FastTextModel} */
    this.model = model;
    addEventListener("message", this.onMessage.bind(this));
  }

  /**
   * Handle any message after the initialization message.
   *
   * @param {Object} data
   * @property {string} data.type - The message type.
   * @property {string} data.message - The message text to identify the language of.
   * @property {number} data.messageId - The ID of the message.
   */
  onMessage({ data }) {
    if (data.type === "initialize") {
      throw new Error("The Language Id engine must not be re-initialized.");
    }
    switch (data.type) {
      case "language-id-request": {
        const { message, messageId } = data;
        try {
          let mostLikelyLanguage = this.model
            .predict(message.trim(), LANGUAGE_COUNT, CONFIDENCE_THRESHOLD)
            .get(0);

          // This should never fail as long as
          // LANGUAGE_COUNT > 1 && CONFIDENCE_THRESHOLD === 0.0
          if (!mostLikelyLanguage) {
            throw new Error("Unable to identify a language");
          }

          const [confidence, languageLabel] = mostLikelyLanguage;

          postMessage({
            type: "language-id-response",
            languageLabel,
            confidence,
            messageId,
          });
        } catch (error) {
          console.error(error);
          postMessage({
            type: "language-id-error",
            messageId,
          });
        }
        break;
      }
      default:
        console.warn("Unknown message type:", data.type);
    }
  }
}
