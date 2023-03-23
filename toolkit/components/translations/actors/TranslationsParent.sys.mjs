/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The pivot language is used to pivot between two different language translations
 * when there is not a model available to translate directly between the two. In this
 * case "en" is common between the various supported models.
 *
 * For instance given the following two models:
 *   "fr" -> "en"
 *   "en" -> "it"
 *
 * You can accomplish:
 *   "fr" -> "it"
 *
 * By doing:
 *   "fr" -> "en" -> "it"
 */
const PIVOT_LANGUAGE = "en";

const lazy = {};

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

ChromeUtils.defineESModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
});

XPCOMUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.translations.logLevel",
    prefix: "Translations",
  });
});

// Do the slow/safe thing of always verifying the signature when the data is
// loaded from the file system. This restriction could be eased in the future if it
// proves to be a performance problem, and the security risk is acceptable.
const VERIFY_SIGNATURES_FROM_FS = true;

/**
 * @typedef {import("../translations").TranslationModelRecord} TranslationModelRecord
 * @typedef {import("../translations").RemoteSettingsClient} RemoteSettingsClient
 * @typedef {import("../translations").LanguageIdEngineMockedPayload} LanguageIdEngineMockedPayload
 * @typedef {import("../translations").LanguageTranslationModelFiles} LanguageTranslationModelFiles
 * @typedef {import("../translations").WasmRecord} WasmRecord
 */

/**
 * The translations parent is used to orchestrate translations in Firefox. It can
 * download the wasm translation engines, and the machine learning language models.
 *
 * See Bug 971044 for more details of planned work.
 */
export class TranslationsParent extends JSWindowActorParent {
  /**
   * The remote settings client that retrieves the language-identification model binary.
   *
   * @type {RemoteSettingsClient | null}
   */
  #languageIdModelsRemoteClient = null;

  /**
   * A map of the TranslationModelRecord["id"] to the record of the model in Remote Settings.
   * Used to coordinate the downloads.
   *
   * @type {Map<string, TranslationModelRecord>}
   */
  #translationModelRecords = new Map();

  /** @type {RemoteSettingsClient | null} */
  #translationModelsRemoteClient = null;

  /** @type {RemoteSettingsClient | null} */
  #translationsWasmRemoteClient = null;

  /**
   * The translation engine can be mocked for testing.
   *
   * @type {Array<{ fromLang: string, toLang: string }>}
   */
  static #mockedLanguagePairs = null;

  /**
   * The language identification engine can be mocked for testing
   * by pre-defining this value.
   *
   * @type {string | null}
   */
  static #mockedLanguageLabel = null;

  /**
   * The language identification engine can be mocked for testing
   * by pre-defining this value.
   *
   * @type {number | null}
   */
  static #mockedLanguageIdConfidence = null;

  actorCreated() {
    if (TranslationsParent.#mockedLanguagePairs) {
      this.sendAsyncMessage("Translations:IsMocked", true);
    }
  }

  async receiveMessage({ name, data }) {
    switch (name) {
      case "Translations:GetBergamotWasmArrayBuffer": {
        return this.#getBergamotWasmArrayBuffer();
      }
      case "Translations:GetLanguageIdModelArrayBuffer": {
        return this.#getLanguageIdModelArrayBuffer();
      }
      case "Translations:GetLanguageIdWasmArrayBuffer": {
        return this.#getLanguageIdWasmArrayBuffer();
      }
      case "Translations:GetLanguageIdEngineMockedPayload": {
        return this.#getLanguageIdEngineMockedPayload();
      }
      case "Translations:GetLanguageTranslationModelFiles": {
        const { fromLanguage, toLanguage } = data;
        const files = await this.getLanguageTranslationModelFiles(
          fromLanguage,
          toLanguage
        );
        if (files) {
          // No pivoting is required.
          return [files];
        }
        // No matching model was found, try to pivot between English.
        const [files1, files2] = await Promise.all([
          this.getLanguageTranslationModelFiles(fromLanguage, PIVOT_LANGUAGE),
          this.getLanguageTranslationModelFiles(PIVOT_LANGUAGE, toLanguage),
        ]);
        if (!files1 || !files2) {
          throw new Error(
            `No language models were found for ${fromLanguage} to ${toLanguage}`
          );
        }
        return [files1, files2];
      }
      case "Translations:GetSupportedLanguages": {
        return this.#getSupportedLanguages();
      }
    }
    return undefined;
  }

  /**
   * Retrieves the language-identification model binary from remote settings.
   *
   * @returns {Promise<ArrayBuffer>}
   */
  async #getLanguageIdModelArrayBuffer() {
    lazy.console.log("Getting language-identification model array buffer.");
    const now = Date.now();
    const client = this.#getLanguageIdModelRemoteClient();

    /** @type {ModelRecord[]} */
    const modelRecords = await client.get({
      // Pull the records from the network so that we never get an empty list.
      syncIfEmpty: true,
      verifySignature: VERIFY_SIGNATURES_FROM_FS,
    });

    if (modelRecords.length === 0) {
      throw new Error(
        "Unable to get language-identification model record from remote settings"
      );
    }

    if (modelRecords.length > 1) {
      lazy.console.error(
        "Expected the language-identification model collection to have only 1 record.",
        modelRecords
      );
    }

    /** @type {{buffer: ArrayBuffer}} */
    const { buffer } = await client.attachments.download(modelRecords[0]);

    const duration = (Date.now() - now) / 1000;
    lazy.console.log(
      `Remote language-identification model loaded in ${duration} seconds.`
    );

    return buffer;
  }

  /**
   * Initializes the RemoteSettingsClient for the language-identification model binary.
   *
   * @returns {RemoteSettingsClient}
   */
  #getLanguageIdModelRemoteClient() {
    if (this.#languageIdModelsRemoteClient) {
      return this.#languageIdModelsRemoteClient;
    }

    /** @type {RemoteSettingsClient} */
    const client = lazy.RemoteSettings("translations-identification-models");
    bypassSignatureVerificationIfDev(client);

    this.#languageIdModelsRemoteClient = client;
    return client;
  }

  /**
   * Retrieves the language-identification wasm binary from remote settings.
   *
   * @returns {Promise<ArrayBuffer>}
   */
  async #getLanguageIdWasmArrayBuffer() {
    const start = Date.now();
    const client = this.#getTranslationsWasmRemoteClient();

    // Load the wasm binary from remote settings, if it hasn't been already.
    lazy.console.log(`Getting remote language-identification wasm binary.`);

    /** @type {WasmRecord[]} */
    const wasmRecords = await client.get({
      // Pull the records from the network so that we never get an empty list.
      syncIfEmpty: true,
      verifySignature: VERIFY_SIGNATURES_FROM_FS,
      // Only get the fasttext-wasm record.
      filters: { name: "fasttext-wasm" },
    });

    if (wasmRecords.length === 0) {
      // The remote settings client provides an empty list of records when there is
      // an error.
      throw new Error(
        "Unable to get language-identification wasm binary from Remote Settings."
      );
    }

    if (wasmRecords.length > 1) {
      lazy.console.error(
        "Expected the language-identification wasm collection to only have 1 record.",
        wasmRecords
      );
    }

    // Unlike the models, greedily download the wasm. It will pull it from a locale
    // cache on disk if it's already been downloaded. Do not retain a copy, as
    // this will be running in the parent process. It's not worth holding onto
    // this much memory, so reload it every time it is needed.

    /** @type {{buffer: ArrayBuffer}} */
    const { buffer } = await client.attachments.download(wasmRecords[0]);

    const duration = (Date.now() - start) / 1000;
    lazy.console.log(
      `Remote language-identification wasm binary loaded in ${duration} seconds.`
    );

    return buffer;
  }

  /**
   * For testing purposes, the LanguageIdEngine can be mocked to always return
   * a pre-determined language label and confidence value.
   *
   * @returns {LanguageIdEngineMockedPayload | null}
   */
  #getLanguageIdEngineMockedPayload() {
    if (
      !TranslationsParent.#mockedLanguageLabel ||
      !TranslationsParent.#mockedLanguageIdConfidence
    ) {
      return null;
    }
    return {
      languageLabel: TranslationsParent.#mockedLanguageLabel,
      confidence: TranslationsParent.#mockedLanguageIdConfidence,
    };
  }

  /**
   * Get the list of languages and their display names, sorted by their display names.
   *
   * @returns {Promise<Array<{ langTag: string, displayName }>>}
   */
  async #getSupportedLanguages() {
    const languages = new Set();
    const languagePairs =
      TranslationsParent.#mockedLanguagePairs ??
      (await this.#getTranslationModelRecords()).values();
    for (const { fromLang } of languagePairs) {
      languages.add(fromLang);
    }

    const displayNames = new Services.intl.DisplayNames(undefined, {
      type: "language",
    });

    return [...languages]
      .map(langTag => ({
        langTag,
        displayName: displayNames.of(langTag),
      }))
      .sort((a, b) => a.displayName.localeCompare(b.displayName));
  }

  /**
   * Lazily initializes the RemoteSettingsClient for the language models.
   *
   * @returns {RemoteSettingsClient}
   */
  #getTranslationModelsRemoteClient() {
    if (this.#translationModelsRemoteClient) {
      return this.#translationModelsRemoteClient;
    }

    /** @type {RemoteSettingsClient} */
    const client = lazy.RemoteSettings("translations-models");

    bypassSignatureVerificationIfDev(client);

    client.on("sync", async ({ data: { created, updated, deleted } }) => {
      // Language model attachments will only be downloaded when they are used.
      lazy.console.log(
        `Remote Settings "sync" event for remote language models `,
        {
          created,
          updated,
          deleted,
        }
      );

      // Remove all the deleted records.
      for (const record of deleted) {
        await client.attachments.deleteDownloaded(record);
        this.#translationModelRecords.delete(record.id);
      }

      // Pre-emptively remove the old downloads, and set the new updated record.
      for (const { old: oldRecord, new: newRecord } of updated) {
        await client.attachments.deleteDownloaded(oldRecord);
        // The language pairs should be the same on the update, but use the old
        // record just in case.
        this.#translationModelRecords.delete(oldRecord.id);
        this.#translationModelRecords.set(newRecord.id, newRecord);
      }

      // Add the new records, but don't download any attachments.
      for (const record of created) {
        this.#translationModelRecords.set(record.id, record);
      }
    });

    return client;
  }

  /**
   * Lazily initializes the model records, and returns the cached ones if they
   * were already retrieved.
   *
   * @returns {Promise<Map<string, TranslationModelRecord>>}
   */
  async #getTranslationModelRecords() {
    if (this.#translationModelRecords.size > 0) {
      return this.#translationModelRecords;
    }

    const now = Date.now();
    const client = this.#getTranslationModelsRemoteClient();

    // Load the models. If no data is present, then there will be an initial sync.
    // Rely on Remote Settings for the syncing strategy for receiving updates.
    lazy.console.log(`Getting remote language models.`);

    /** @type {TranslationModelRecord[]} */
    const translationModelRecords = await client.get({
      // Pull the records from the network so that we never get an empty list.
      syncIfEmpty: true,
      verifySignature: VERIFY_SIGNATURES_FROM_FS,
    });

    for (const record of translationModelRecords) {
      this.#translationModelRecords.set(record.id, record);
    }

    const duration = (Date.now() - now) / 1000;
    lazy.console.log(
      `Remote language models loaded in ${duration} seconds.`,
      translationModelRecords
    );

    return this.#translationModelRecords;
  }

  /**
   * Lazily initializes the RemoteSettingsClient for the downloaded wasm binary data.
   *
   * @returns {RemoteSettingsClient}
   */
  #getTranslationsWasmRemoteClient() {
    if (this.#translationsWasmRemoteClient) {
      return this.#translationsWasmRemoteClient;
    }
    /** @type {RemoteSettingsClient} */
    const client = lazy.RemoteSettings("translations-wasm");

    bypassSignatureVerificationIfDev(client);

    client.on("sync", async ({ data: { created, updated, deleted } }) => {
      lazy.console.log(`"sync" event for remote bergamot wasm `, {
        created,
        updated,
        deleted,
      });

      // Remove all the deleted records.
      for (const record of deleted) {
        await client.attachments.deleteDownloaded(record);
      }

      // Remove any updated records, and download the new ones.
      for (const { old: oldRecord } of updated) {
        await client.attachments.deleteDownloaded(oldRecord);
      }

      // Do nothing for the created records.
    });

    this.#translationsWasmRemoteClient = client;
    return client;
  }

  /**
   * Bergamot is the translation engine that has been compiled to wasm. It is shipped
   * to the user via Remote Settings.
   *
   * https://github.com/mozilla/bergamot-translator/
   */
  /**
   * @returns {Promise<ArrayBuffer>}
   */
  async #getBergamotWasmArrayBuffer() {
    const start = Date.now();
    const client = this.#getTranslationsWasmRemoteClient();

    // Load the wasm binary from remote settings, if it hasn't been already.
    lazy.console.log(`Getting remote bergamot-translator wasm records.`);

    /** @type {WasmRecord[]} */
    const wasmRecords = await client.get({
      // Pull the records from the network so that we never get an empty list.
      syncIfEmpty: true,
      verifySignature: VERIFY_SIGNATURES_FROM_FS,
      // Only get the bergamot-translator record.
      filters: { name: "bergamot-translator" },
    });

    if (wasmRecords.length === 0) {
      // The remote settings client provides an empty list of records when there is
      // an error.
      throw new Error(
        "Unable to get the bergamot translator from Remote Settings."
      );
    }

    if (wasmRecords.length > 1) {
      lazy.console.error(
        "Expected the bergamot-translator to only have 1 record.",
        wasmRecords
      );
    }

    // Unlike the models, greedily download the wasm. It will pull it from a locale
    // cache on disk if it's already been downloaded. Do not retain a copy, as
    // this will be running in the parent process. It's not worth holding onto
    // this much memory, so reload it every time it is needed.

    /** @type {{buffer: ArrayBuffer}} */
    const { buffer } = await client.attachments.download(wasmRecords[0]);

    const duration = Date.now() - start;
    lazy.console.log(
      `"bergamot-translator" wasm binary loaded in ${duration / 1000} seconds`
    );

    return buffer;
  }

  /**
   * Gets the language model files in an array buffer by downloading attachments from
   * Remote Settings, or retrieving them from the local cache. Each translation
   * requires multiple files.
   *
   * Results are only returned if the model is found.
   *
   * @param {string} fromLanguage
   * @param {string} toLanguage
   * @param {boolean} withQualityEstimation
   * @returns {null | LanguageTranslationModelFiles}
   */
  async getLanguageTranslationModelFiles(
    fromLanguage,
    toLanguage,
    withQualityEstimation = false
  ) {
    const client = this.#getTranslationModelsRemoteClient();

    lazy.console.log(
      `Beginning model downloads: "${fromLanguage}" to "${toLanguage}"`
    );

    const records = [...(await this.#getTranslationModelRecords()).values()];

    /** @type {LanguageTranslationModelFiles} */
    let results;

    // Use Promise.all to download (or retrieve from cache) the model files in parallel.
    await Promise.all(
      records.map(async record => {
        if (record.fileType === "qualityModel" && !withQualityEstimation) {
          // Do not include the quality models if they aren't needed.
          return;
        }

        if (record.fromLang !== fromLanguage || record.toLang !== toLanguage) {
          // Only use models that match.
          return;
        }

        if (!results) {
          results = {};
        }

        const start = Date.now();

        // Download or retrieve from the local cache:

        /** @type {{buffer: ArrayBuffer }} */
        const { buffer } = await client.attachments.download(record);

        results[record.fileType] = {
          buffer,
          record,
        };

        const duration = Date.now() - start;
        lazy.console.log(
          `Translation model fetched in ${duration / 1000} seconds:`,
          record.fromLang,
          record.toLang,
          record.fileType
        );
      })
    );

    if (!results) {
      // No model files were found, pivoting will be required.
      return null;
    }

    // Validate that all of the files we expected were actually available and
    // downloaded.

    if (!results.model) {
      throw new Error(
        `No model file was found for "${fromLanguage}" to "${toLanguage}."`
      );
    }

    if (!results.lex) {
      throw new Error(
        `No lex file was found for "${fromLanguage}" to "${toLanguage}."`
      );
    }

    if (withQualityEstimation && !results.qualityModel) {
      throw new Error(
        `No quality file was found for "${fromLanguage}" to "${toLanguage}."`
      );
    }

    if (results.vocab) {
      if (results.srcvocab) {
        throw new Error(
          `A srcvocab and vocab file were both included for "${fromLanguage}" to "${toLanguage}." Only one is needed.`
        );
      }
      if (results.trgvocab) {
        throw new Error(
          `A trgvocab and vocab file were both included for "${fromLanguage}" to "${toLanguage}." Only one is needed.`
        );
      }
    } else if (!results.srcvocab || !results.srcvocab) {
      throw new Error(
        `No vocab files were provided for "${fromLanguage}" to "${toLanguage}."`
      );
    }

    return results;
  }

  /**
   * For testing purposes, allow the Translations Engine to be mocked. If called
   * with `null` the mock is removed.
   *
   * @param {null | Array<{ fromLang: string, toLang: string }>} languagePairs
   */
  static mockLanguagePairs(languagePairs) {
    TranslationsParent.#mockedLanguagePairs = languagePairs;
    if (languagePairs) {
      lazy.console.log("Mocking language pairs", languagePairs);
    } else {
      lazy.console.log("Removing language pair mocks");
    }
  }

  /**
   * For testing purposes, allow the LanguageIdEngine to be mocked. If called
   * with `null` in each argument, the mock is removed.
   *
   * @param {string} languageLabel - The two-character language label.
   * @param {number} confidence  - The confidence score of the detected language.
   */
  static mockLanguageIdentification(languageLabel, confidence) {
    TranslationsParent.#mockedLanguageLabel = languageLabel;
    TranslationsParent.#mockedLanguageIdConfidence = confidence;
    if (languageLabel) {
      lazy.console.log("Mocking detected language label", languageLabel);
    } else {
      lazy.console.log("Removing detected-language label mock");
    }
    if (languageLabel) {
      lazy.console.log("Mocking detected language confidence", confidence);
    } else {
      lazy.console.log("Removing detected-language confidence mock");
    }
  }
}

/**
 * The signature verification can break on the Dev server. Bypass it to ensure new
 * language models can always be tested. On Prod and Staging the signatures will
 * always be verified.
 *
 * @param {RemoteSettingsClient} client
 */
function bypassSignatureVerificationIfDev(client) {
  let host;
  try {
    const url = new URL(Services.prefs.getCharPref("services.settings.server"));
    host = url.host;
  } catch (error) {}

  if (host === "settings.dev.mozaws.net") {
    console.warn(
      "The translations is set to the Remote Settings dev server. It's bypassing " +
        "the signature verification."
    );
    client.verifySignature = false;
  }
}
