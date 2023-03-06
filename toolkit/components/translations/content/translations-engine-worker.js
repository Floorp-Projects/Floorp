/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/chrome-worker */
"use strict";

/**
 * @typedef {import("../translations").Bergamot} Bergamot
 * @typedef {import("../translations").LanguageTranslationModelFiles} LanguageTranslationModelFiles
 */

/* global loadBergamot */
importScripts("chrome://global/content/translations/bergamot-translator.js");

// Respect the preference "browser.translations.logLevel".
let _isLoggingEnabled = false;
function log(...args) {
  if (_isLoggingEnabled) {
    console.log("Translations:", ...args);
  }
}

// Throw Promise rejection errors so that they are visible in the console.
self.addEventListener("unhandledrejection", event => {
  throw event.reason;
});

/**
 * The alignment for each file type, file type strings should be same as in the
 * model registry.
 */
const MODEL_FILE_ALIGNMENTS = {
  model: 256,
  lex: 64,
  vocab: 64,
  qualityModel: 64,
  srcvocab: 64,
  trgvocab: 64,
};

/**
 * Initialize the engine, and get it ready to handle translation requests.
 * The "initialize" message must be received before any other message handling
 * requests will be processed.
 */
addEventListener("message", handleInitializationMessage);

async function handleInitializationMessage({ data }) {
  if (data.type !== "initialize") {
    console.error(
      "The TranslationEngine worker received a message before it was initialized."
    );
    return;
  }

  try {
    const { fromLanguage, toLanguage, enginePayload, isLoggingEnabled } = data;

    if (!fromLanguage) {
      throw new Error('Worker initialization missing "fromLanguage"');
    }
    if (!toLanguage) {
      throw new Error('Worker initialization missing "toLanguage"');
    }

    if (isLoggingEnabled) {
      // Respect the "browser.translations.logLevel" preference.
      _isLoggingEnabled = true;
    }

    let engine;
    if (enginePayload) {
      const { bergamotWasmArrayBuffer, languageModelFiles } = enginePayload;
      const bergamot = await BergamotUtils.initializeWasm(
        bergamotWasmArrayBuffer
      );
      engine = new Engine(
        fromLanguage,
        toLanguage,
        bergamot,
        languageModelFiles
      );
    } else {
      // The engine is testing mode, and no Bergamot wasm is available.
      engine = new MockedEngine(fromLanguage, toLanguage);
    }

    handleMessages(engine);
    postMessage({ type: "initialization-success" });
  } catch (error) {
    // TODO (Bug 1813781) - Handle this error in the UI.
    console.error(error);
    postMessage({ type: "initialization-error", error: error?.message });
  }

  removeEventListener("message", handleInitializationMessage);
}

/**
 * Sets up the message handling for the worker.
 *
 * @param {Engine | MockedEngine} engine
 */
function handleMessages(engine) {
  addEventListener("message", ({ data }) => {
    try {
      if (data.type === "initialize") {
        throw new Error("The Translations engine must not be re-initialized.");
      }
      log("Received message", data);

      switch (data.type) {
        case "translation-request": {
          const { messageBatch, messageId, isHTML } = data;
          try {
            const translations = engine.translate(messageBatch, isHTML);
            postMessage({
              type: "translation-response",
              translations,
              messageId,
            });
          } catch (error) {
            console.error(error);
            let message = "An error occurred in the engine worker.";
            if (typeof error?.message === "string") {
              message = error.message;
            }
            let stack = "(no stack)";
            if (typeof error?.stack === "string") {
              stack = error.stack;
            }
            postMessage({
              type: "translation-error",
              error: { message, stack },
              messageId,
            });
          }
          break;
        }
        default:
          console.warn("Unknown message type:", data.type);
      }
    } catch (error) {
      // Ensure the unexpected errors are surfaced in the console.
      console.error(error);
    }
  });
}

/**
 * The Engine is created once for a language pair. The initialization process copies the
 * ArrayBuffers for the language buffers from JS-managed ArrayBuffers, to aligned
 * internal memory for the wasm heap.
 *
 * After this the ArrayBuffers are discarded and GC'd. This file should be managed
 * from the TranslationsEngine class on the main thread.
 *
 * This class starts listening for messages only after the Bergamot engine has been
 * fully initialized.
 */
class Engine {
  /**
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @param {Bergamot} bergamot
   * @param {Array<LanguageTranslationModelFiles>} languageTranslationModelFiles
   */
  constructor(
    fromLanguage,
    toLanguage,
    bergamot,
    languageTranslationModelFiles
  ) {
    /** @type {string} */
    this.fromLanguage = fromLanguage;
    /** @type {string} */
    this.toLanguage = toLanguage;
    /** @type {Bergamot} */
    this.bergamot = bergamot;
    /** @type {Bergamot["TranslationModel"][]} */
    this.languageTranslationModels = languageTranslationModelFiles.map(
      languageTranslationModelFiles =>
        BergamotUtils.constructSingleTranslationModel(
          bergamot,
          languageTranslationModelFiles
        )
    );

    /** @type {Bergamot["BlockingService"]} */
    this.translationService = new bergamot.BlockingService({
      // Caching is disabled (see https://github.com/mozilla/firefox-translations/issues/288)
      cacheSize: 0,
    });
  }

  /**
   * Run the translation models to perform a batch of message translations.
   *
   * @param {string[]} messageBatch
   * @param {boolean} isHTML
   * @param {boolean} withQualityEstimation
   * @returns {string[]}
   */
  translate(messageBatch, isHTML, withQualityEstimation = false) {
    let response;
    const { messages, options } = BergamotUtils.getTranslationArgs(
      this.bergamot,
      messageBatch,
      isHTML,
      withQualityEstimation
    );
    try {
      if (messages.size() === 0) {
        return [];
      }

      /** @type {Bergamot["VectorResponse"]} */
      let responses;

      if (this.languageTranslationModels.length === 1) {
        responses = this.translationService.translate(
          this.languageTranslationModels[0],
          messages,
          options
        );
      } else if (this.languageTranslationModels.length === 2) {
        responses = this.translationService.translateViaPivoting(
          this.languageTranslationModels[0],
          this.languageTranslationModels[1],
          messages,
          options
        );
      } else {
        throw new Error(
          "Too many models were provided to the translation worker."
        );
      }

      // Extract JavaScript values out of the vector.
      return BergamotUtils.mapVector(responses, response =>
        response.getTranslatedText()
      );
    } finally {
      // Free up any memory that was allocated. This will always run.
      messages?.delete();
      options?.delete();
      response?.delete();
    }
  }
}

/**
 * Static utilities to help work with the Bergamot wasm module.
 */
class BergamotUtils {
  /**
   * Construct a single translation model.
   *
   * @param {Bergamot} bergamot
   * @param {LanguageTranslationModelFiles} languageTranslationModelFiles
   * @returns {Bergamot["TranslationModel"]}
   */
  static constructSingleTranslationModel(
    bergamot,
    languageTranslationModelFiles
  ) {
    log(`Constructing translation model.`);

    const {
      model,
      lex,
      vocab,
      qualityModel,
      srcvocab,
      trgvocab,
    } = BergamotUtils.allocateModelMemory(
      bergamot,
      languageTranslationModelFiles
    );

    // Transform the bytes to mb, like "10.2mb"
    const getMemory = memory => `${Math.floor(memory.size() / 100_000) / 10}mb`;

    let memoryLog = `Model memory sizes in wasm heap:`;
    memoryLog += `\n  Model: ${getMemory(model)}`;
    memoryLog += `\n  Shortlist: ${getMemory(lex)}`;

    // Set up the vocab list, which could either be a single "vocab" model, or a
    // "srcvocab" and "trgvocab" pair.
    const vocabList = new bergamot.AlignedMemoryList();

    if (vocab) {
      vocabList.push_back(vocab);
      memoryLog += `\n  Vocab: ${getMemory(vocab)}`;
    } else if (srcvocab && trgvocab) {
      vocabList.push_back(srcvocab);
      vocabList.push_back(trgvocab);
      memoryLog += `\n  Src Vocab: ${getMemory(srcvocab)}`;
      memoryLog += `\n  Trg Vocab: ${getMemory(trgvocab)}`;
    } else {
      throw new Error("Vocabulary key is not found.");
    }

    if (qualityModel) {
      memoryLog += `\n  QualityModel: ${getMemory(qualityModel)}\n`;
    }

    const config = BergamotUtils.generateTextConfig({
      "beam-size": "1",
      normalize: "1.0",
      "word-penalty": "0",
      "max-length-break": "128",
      "mini-batch-words": "1024",
      workspace: "128",
      "max-length-factor": "2.0",
      "skip-cost": (!qualityModel).toString(),
      "cpu-threads": "0",
      quiet: "true",
      "quiet-translation": "true",
      "gemm-precision": languageTranslationModelFiles.model.record.name.endsWith(
        "intgemm8.bin"
      )
        ? "int8shiftAll"
        : "int8shiftAlphaAll",
      alignment: "soft",
    });

    log(`Bergamot translation model config: ${config}`);
    log(memoryLog);

    return new bergamot.TranslationModel(
      config,
      model,
      lex,
      vocabList,
      qualityModel ?? null
    );
  }

  /**
   * The models must be placed in aligned memory that the Bergamot wasm module has access
   * to. This function copies over the model blobs into this memory space.
   *
   * @param {Bergamot} bergamot
   * @param {LanguageTranslationModelFiles} languageTranslationModelFiles
   * @returns {LanguageTranslationModelFilesAligned}
   */
  static allocateModelMemory(bergamot, languageTranslationModelFiles) {
    /** @type {LanguageTranslationModelFilesAligned} */
    const results = {};

    for (const [fileType, file] of Object.entries(
      languageTranslationModelFiles
    )) {
      const alignment = MODEL_FILE_ALIGNMENTS[fileType];
      if (!alignment) {
        throw new Error(`Unknown file type: "${fileType}"`);
      }

      const alignedMemory = new bergamot.AlignedMemory(
        file.buffer.byteLength,
        alignment
      );

      alignedMemory.getByteArrayView().set(new Uint8Array(file.buffer));

      results[fileType] = alignedMemory;
    }

    return results;
  }

  /**
   * Initialize the Bergamot translation engine. It is a wasm compiled version of the
   * Marian translation software. The wasm is delivered remotely to cut down on binary size.
   *
   * https://github.com/mozilla/bergamot-translator/
   *
   * @param {ArrayBuffer} wasmBinary
   * @returns {Promise<Bergamot>}
   */
  static initializeWasm(wasmBinary) {
    return new Promise((resolve, reject) => {
      /** @type {number} */
      let start = performance.now();

      /** @type {Bergamot} */
      const bergamot = loadBergamot({
        preRun: [],
        onAbort() {
          reject(new Error("Error loading Bergamot wasm module."));
        },
        onRuntimeInitialized: async () => {
          const duration = performance.now() - start;
          log(
            `Bergamot wasm runtime initialized in ${duration / 1000} seconds.`
          );
          // Await at least one microtask so that the captured `bergamot` variable is
          // fully initialized.
          await Promise.resolve();
          resolve(bergamot);
        },
        wasmBinary,
      });
    });
  }

  /**
   * Maps the Bergamot Vector to a JS array
   *
   * @param {Bergamot["Vector"]} vector
   * @param {Function} fn
   * @returns {Array}
   */
  static mapVector(vector, fn) {
    const result = [];
    for (let index = 0; index < vector.size(); index++) {
      result.push(fn(vector.get(index), index));
    }
    return result;
  }

  /**
   * Generate a config for the Marian translation service. It requires specific whitespace.
   *
   * https://marian-nmt.github.io/docs/cmd/marian-decoder/
   *
   * @param {Record<string, string>} config
   * @returns {string}
   */
  static generateTextConfig(config) {
    const indent = "            ";
    let result = "\n";

    for (const [key, value] of Object.entries(config)) {
      result += `${indent}${key}: ${value}\n`;
    }

    return result + indent;
  }

  /**
   * JS objects need to be translated into wasm objects to configure the translation engine.
   *
   * @param {Bergamot} bergamot
   * @param {string[]} messageBatch
   * @param {boolean} withQualityEstimation
   * @returns {{ messages: Bergamot["VectorString"], options: Bergamot["VectorResponseOptions"] }}
   */
  static getTranslationArgs(
    bergamot,
    messageBatch,
    isHTML,
    withQualityEstimation
  ) {
    const messages = new bergamot.VectorString();
    const options = new bergamot.VectorResponseOptions();
    for (const message of messageBatch) {
      // Empty paragraphs break the translation.
      if (message.trim() === "") {
        continue;
      }

      if (withQualityEstimation && !isHTML) {
        // Bergamot only supports quality estimates with HTML. Purely text content can
        // be translated by escaping it as HTML. See:
        // https://github.com/mozilla/firefox-translations/blob/431e0d21f22694c1cbc0ff965820d9780cdaeea8/extension/controller/translation/translationWorker.js#L146-L158
        throw new Error(
          "Quality estimates on non-hTML is not curently supported."
        );
      }

      messages.push_back(message);
      options.push_back({
        qualityScores: withQualityEstimation,
        alignment: true,
        html: isHTML,
      });
    }
    return { messages, options };
  }
}

/**
 * For testing purposes, provide a fully mocked engine. This allows for easy integration
 * testing of the UI, without having to rely on downloading remote models and remote
 * wasm binaries.
 */
class MockedEngine {
  /**
   * @param {string} fromLanguage
   * @param {string} toLanguage
   */
  constructor(fromLanguage, toLanguage) {
    /** @type {string} */
    this.fromLanguage = fromLanguage;
    /** @type {string} */
    this.toLanguage = toLanguage;
  }

  /**
   * Create a fake translation of the text.
   *
   * @param {string[]} messageBatch
   * @param {bool} isHTML
   * @returns {string}
   */
  translate(messageBatch, isHTML) {
    return messageBatch.map(message => {
      // Note when an HTML translations is requested.
      let html = isHTML ? ", html" : "";
      message = message.toUpperCase();

      return `${message} [${this.fromLanguage} to ${this.toLanguage}${html}]`;
    });
  }
}
