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
addEventListener("message", handleInitializationMessage);

/**
 * Initialize the engine, and get it ready to handle language identification requests.
 * The "initialize" message must be received before any other message handling
 * requests will be processed.
 *
 * @param {Object} data
 * @property {string} data.type - The message type, expects "initialize".
 * @property {ArrayBuffer} data.wasmBuffer - The buffer containing the wasm binary.
 * @property {ArrayBuffer} data.modelBuffer - The buffer containing the language-id model binary.
 * @property {string} data.languageLabel - The mocked language label value (only present when mocking).
 * @property {number} data.confidence - The mocked confidence value (only present when mocking).
 * @property {boolean} data.isLoggingEnabled
 */
async function handleInitializationMessage({ data }) {
  if (data.type !== "initialize") {
    throw new Error(
      "The LanguageIdEngine worker received a message before it was initialized."
    );
  }

  try {
    const { isLoggingEnabled } = data;
    if (isLoggingEnabled) {
      // Respect the "browser.translations.logLevel" preference.
      _isLoggingEnabled = true;
    }

    /** @type {LanguageIdEngine | MockedLanguageIdEngine} */
    let languageIdEngine;
    if (isMockedDataPayload(data)) {
      languageIdEngine = initializeMockedLanguageIdEngine(data);
    } else {
      languageIdEngine = await initializeLanguageIdEngine(data);
    }

    handleMessages(languageIdEngine);
    postMessage({ type: "initialization-success" });
  } catch (error) {
    console.error(error);
    postMessage({ type: "initialization-error", error: error?.message });
  }

  removeEventListener("message", handleInitializationMessage);
}

/**
 * Returns true if this data payload contains mocked values for the languageLabel
 * and the confidence, otherwise returns false.
 *
 * @property {string} data.languageLabel
 * @property {number} data.confidence
 * @returns {boolean}
 */
function isMockedDataPayload(data) {
  let { languageLabel, confidence } = data;
  return languageLabel && confidence;
}

/**
 * Initializes the fastText wasm runtime and returns the fastText model.
 *
 * @param {ArrayBuffer} data.wasmBuffer - The buffer containing the wasm binary.
 * @param {ArrayBuffer} data.modelBuffer - The buffer containing the language-id model binary.
 * @returns {FastTextModel}
 */
function initializeFastTextModel(modelBuffer, wasmBuffer) {
  return new Promise((resolve, reject) => {
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
}

/**
 * Initialize the LanguageIdEngine from the data payload by loading
 * the fastText wasm runtime and model and constructing the engine.
 *
 * @param {Object} data
 * @property {ArrayBuffer} data.wasmBuffer - The buffer containing the wasm binary.
 * @property {ArrayBuffer} data.modelBuffer - The buffer containing the language-id model binary.
 */
async function initializeLanguageIdEngine(data) {
  const { modelBuffer, wasmBuffer } = data;
  if (!modelBuffer) {
    throw new Error('LanguageIdEngine initialization missing "modelBuffer"');
  }
  if (!wasmBuffer) {
    throw new Error('LanguageIdEngine initialization missing "wasmBuffer"');
  }
  const model = await initializeFastTextModel(modelBuffer, wasmBuffer);
  return new LanguageIdEngine(model);
}

/**
 * Initialize the MockedLanguageIdEngine from the data payload by loading
 * assigning it a pre-determined languageLabel and confidence values.
 *
 * @param {Object} data
 * @property {string} data.languageLabel - The pre-determined mocked language label
 * @property {number} data.confidence - the pre-determined mocked confidence
 */
function initializeMockedLanguageIdEngine(data) {
  const { languageLabel, confidence } = data;
  if (!languageLabel) {
    throw new Error('MockedLanguageIdEngine missing "languageLabel"');
  }
  if (!confidence) {
    throw new Error('MockedLanguageIdEngine missing "confidence"');
  }
  return new MockedLanguageIdEngine(languageLabel, confidence);
}

/**
 * Sets up the message handling for the lworker.
 *
 * @param {LanguageIdEngine | MockedLanguageIdEngine} languageIdEngine
 */
function handleMessages(languageIdEngine) {
  /**
   * Handle any message after the initialization message.
   *
   * @param {Object} data
   * @property {string} data.type - The message type.
   * @property {string} data.message - The message text to identify the language of.
   * @property {number} data.messageId - The ID of the message.
   */
  addEventListener("message", ({ data }) => {
    try {
      if (data.type === "initialize") {
        throw new Error(
          "The language-identification engine must not be re-initialized."
        );
      }
      switch (data.type) {
        case "language-id-request": {
          const { message, messageId } = data;
          try {
            const [
              confidence,
              languageLabel,
            ] = languageIdEngine.identifyLanguage(message);
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
        default: {
          console.warn("Unknown message type:", data.type);
        }
      }
    } catch (error) {
      // Ensure the unexpected errors are surfaced in the console.
      console.error(error);
    }
  });
}

/**
 * The LanguageIdEngine wraps around a machine-learning model that can identify text
 * as being written in a given human language. The engine is responsible for invoking
 * model and returning the language label in the format that is expected by firefox
 * translations code.
 */
class LanguageIdEngine {
  /** @type {FastTextModel} */
  #model;

  /**
   * @param {FastTextModel} model
   */
  constructor(model) {
    this.#model = model;
  }

  /**
   * Formats the language label returned by the language-identification model
   * to conform to the correct two-character language tags.
   *
   * The current model returns labels of the format "__label_xx" where the last
   * two characters are the two-character language tag.
   *
   * As such, this function strips of those final two characters.
   * Updating the language-identification model may require updating this function.
   *
   * @param {string} label
   * @returns {string}
   */
  #formatLanguageLabel(label) {
    return label.slice(-2);
  }

  /**
   * Identifies the human language in which the message is written and returns
   * the two-letter language label of the language it is determined to be along
   * with a rating of how confident the model is that the label is correct.
   *
   * @param {string} message
   * @returns {Array<number | string>} An array containing the confidence and language label.
   * The confidence is a number between 0 and 1, representing a percentage.
   * The language label is a two-character label such as "en" for English.
   * e.g. [0.87, "en"]
   */
  identifyLanguage(message) {
    const mostLikelyLanguageData = this.#model
      .predict(message.trim(), LANGUAGE_COUNT, CONFIDENCE_THRESHOLD)
      .get(0);

    // This should never fail as long as
    // LANGUAGE_COUNT > 1 && CONFIDENCE_THRESHOLD === 0.0
    if (!mostLikelyLanguageData) {
      throw new Error("Unable to identify a language");
    }

    const [confidence, languageLabel] = mostLikelyLanguageData;
    return [confidence, this.#formatLanguageLabel(languageLabel)];
  }
}

/**
 * For testing purposes, provide a fully mocked engine. This allows for easy integration
 * testing of the UI, without having to rely on downloading remote models and remote
 * wasm binaries.
 */
class MockedLanguageIdEngine {
  /** @type {string} */
  #languageLabel;
  /** @type {number} */
  #confidence;

  /**
   * @param {string} languageLabel
   * @param {number} confidence
   */
  constructor(languageLabel, confidence) {
    this.#languageLabel = languageLabel;
    this.#confidence = confidence;
  }

  /**
   * Mocks identifying a lgnauge by returning the mocked engine's pre-determined
   * language label and confidence values.
   */
  identifyLanguage(_message) {
    return [this.#confidence, this.#languageLabel];
  }
}
