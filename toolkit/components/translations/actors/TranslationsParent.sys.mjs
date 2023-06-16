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

const TRANSLATIONS_PERMISSION = "translations";
const ALWAYS_TRANSLATE_LANGS_PREF =
  "browser.translations.alwaysTranslateLanguages";
const NEVER_TRANSLATE_LANGS_PREF =
  "browser.translations.neverTranslateLanguages";

const lazy = {};

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

ChromeUtils.defineESModuleGetters(lazy, {
  RemoteSettings: "resource://services-settings/remote-settings.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

XPCOMUtils.defineLazyGetter(lazy, "console", () => {
  return console.createInstance({
    maxLogLevelPref: "browser.translations.logLevel",
    prefix: "Translations",
  });
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "translationsEnabledPref",
  "browser.translations.enable"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "autoTranslatePagePref",
  "browser.translations.autoTranslate"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "chaosErrorsPref",
  "browser.translations.chaos.errors"
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "chaosTimeoutMSPref",
  "browser.translations.chaos.timeoutMS"
);

/**
 * Returns the always-translate language tags as an array.
 */
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "alwaysTranslateLangTags",
  ALWAYS_TRANSLATE_LANGS_PREF,
  /* aDefaultPrefValue */ "",
  /* onUpdate */ null,
  /* aTransform */ rawLangTags => (rawLangTags ? rawLangTags.split(",") : [])
);

/**
 * Returns the never-translate language tags as an array.
 */
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "neverTranslateLangTags",
  NEVER_TRANSLATE_LANGS_PREF,
  /* aDefaultPrefValue */ "",
  /* onUpdate */ null,
  /* aTransform */ rawLangTags => (rawLangTags ? rawLangTags.split(",") : [])
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "simulateUnsupportedEnginePref",
  "browser.translations.simulateUnsupportedEngine"
);

// At this time the signatures of the files are not being checked when they are being
// loaded from disk. This signature check involves hitting the network, and translations
// are explicitly an offline-capable feature. See Bug 1827265 for re-enabling this
// check.
const VERIFY_SIGNATURES_FROM_FS = false;

/**
 * @typedef {import("../translations").TranslationModelRecord} TranslationModelRecord
 * @typedef {import("../translations").RemoteSettingsClient} RemoteSettingsClient
 * @typedef {import("../translations").LanguageIdEngineMockedPayload} LanguageIdEngineMockedPayload
 * @typedef {import("../translations").LanguageTranslationModelFiles} LanguageTranslationModelFiles
 * @typedef {import("../translations").WasmRecord} WasmRecord
 * @typedef {import("../translations").LangTags} LangTags
 * @typedef {import("../translations").LanguagePair} LanguagePair
 * @typedef {import("../translations").SupportedLanguages} SupportedLanguages
 * @typedef {import("../translations").LanguageIdModelRecord} LanguageIdModelRecord
 * @typedef {import("../translations").TranslationErrors} TranslationErrors
 */

/**
 * @typedef {Object} TranslationPair
 * @prop {string} fromLanguage
 * @prop {string} toLanguage
 * @prop {string} [fromDisplayLanguage]
 * @prop {string} [toDisplayLanguage]
 */

/**
 * The translations parent is used to orchestrate translations in Firefox. It can
 * download the wasm translation engines, and the machine learning language models.
 *
 * See Bug 971044 for more details of planned work.
 */
export class TranslationsParent extends JSWindowActorParent {
  /**
   * Contains the state that would affect UI. Anytime this state is changed, a dispatch
   * event is sent so that UI can react to it. The actor is inside of /toolkit and
   * needs a way of notifying /browser code (or other users) of when the state changes.
   *
   * @type {TranslationsLanguageState}
   */
  languageState;

  actorCreated() {
    this.languageState = new TranslationsLanguageState(this);

    if (TranslationsParent.#translateOnPageReload) {
      // The actor was recreated after a page reload, start the translation.
      const { fromLanguage, toLanguage } =
        TranslationsParent.#translateOnPageReload;
      TranslationsParent.#translateOnPageReload = null;

      lazy.console.log(
        `Translating on a page reload from "${fromLanguage}" to "${toLanguage}".`
      );

      this.translate(fromLanguage, toLanguage);
    }
  }

  /**
   * The remote settings client that retrieves the language-identification model binary.
   *
   * @type {RemoteSettingsClient | null}
   */
  static #languageIdModelsRemoteClient = null;

  /**
   * A map of the TranslationModelRecord["id"] to the record of the model in Remote Settings.
   * Used to coordinate the downloads.
   *
   * @type {Map<string, TranslationModelRecord>}
   */
  #translationModelRecords = new Map();

  /**
   * The RemoteSettingsClient that downloads the translation models.
   *
   * @type {RemoteSettingsClient | null}
   */
  static #translationModelsRemoteClient = null;

  /**
   * The RemoteSettingsClient that downloads the wasm binaries.
   *
   * @type {RemoteSettingsClient | null}
   */
  static #translationsWasmRemoteClient = null;

  /**
   * If "browser.translations.autoTranslate" is set to "true" then the page will
   * auto-translate. A user can restore the page to the original UI. This flag indicates
   * that an auto-translate should be skipped.
   */
  static #isPageRestoredForAutoTranslate = false;

  /**
   * Allows the actor's behavior to be changed when the translations engine is mocked via
   * a dummy RemoteSettingsClient.
   *
   * @type {bool}
   */
  static #isTranslationsEngineMocked = false;

  /**
   * The language identification engine can be mocked for testing
   * by pre-defining this value.
   *
   * @type {string | null}
   */
  static #mockedLangTag = null;

  /**
   * The language identification engine can be mocked for testing
   * by pre-defining this value.
   *
   * @type {number | null}
   */
  static #mockedLanguageIdConfidence = null;

  /**
   * @type {null | Promise<boolean>}
   */
  static #isTranslationsEngineSupported = null;

  /**
   * When reloading the page, store the translation pair that needs translating.
   *
   * @type {null | TranslationPair}
   */
  static #translateOnPageReload = null;

  /**
   * An ordered list of preferred languages based on:
   *   1. App languages
   *   2. Web requested languages
   *   3. OS language
   *
   * @type {null | string[]}
   */
  static #preferredLanguages = null;
  static #observingLanguages = false;

  // On a fast connection, 10 concurrent downloads were measured to be the fastest when
  // downloading all of the language files.
  static MAX_CONCURRENT_DOWNLOADS = 10;
  static MAX_DOWNLOAD_RETRIES = 3;

  /**
   * Detect if Wasm SIMD is supported, and cache the value. It's better to check
   * for support before downloading large binary blobs to a user who can't even
   * use the feature. This function also respects mocks and simulating unsupported
   * engines.
   *
   * @type {Promise<boolean>}
   */
  static getIsTranslationsEngineSupported() {
    if (lazy.simulateUnsupportedEnginePref) {
      // Use the non-lazy console.log so that the user is always informed as to why
      // the translations engine is not working.
      console.log(
        "Translations: The translations engine is disabled through the pref " +
          '"browser.translations.simulateUnsupportedEngine".'
      );

      // The user is manually testing unsupported engines.
      return Promise.resolve(false);
    }

    if (TranslationsParent.#isTranslationsEngineMocked) {
      // A mocked translations engine is always supported.
      return Promise.resolve(true);
    }

    if (TranslationsParent.#isTranslationsEngineSupported === null) {
      TranslationsParent.#isTranslationsEngineSupported = detectSimdSupport();

      TranslationsParent.#isTranslationsEngineSupported.then(
        isSupported => () => {
          // Use the non-lazy console.log so that the user is always informed as to why
          // the translations engine is not working.
          if (!isSupported) {
            console.log(
              "Translations: The translations engine is not supported on your device as " +
                "it does not support Wasm SIMD operations."
            );
          }
        }
      );
    }

    return TranslationsParent.#isTranslationsEngineSupported;
  }

  /**
   * Only translate pages that match certain protocols, that way internal pages like
   * about:* pages will not be translated.
   * @param {string} url
   */
  static isRestrictedPage(url) {
    // Keep this logic up to date with TranslationsChild.prototype.#isRestrictedPage.
    return !(
      url.startsWith("http://") ||
      url.startsWith("https://") ||
      url.startsWith("file:///")
    );
  }

  static #resetPreferredLanguages() {
    TranslationsParent.#preferredLanguages = null;
    TranslationsParent.getPreferredLanguages();
  }

  static async observe(_subject, topic, _data) {
    switch (topic) {
      case "nsPref:changed":
      case "intl:app-locales-changed": {
        this.#resetPreferredLanguages();
        break;
      }
      default:
        throw new Error("Unknown observer event", topic);
    }
  }

  /**
   * Provide a way for tests to override the system locales.
   * @type {null | string[]}
   */
  mockedSystemLocales = null;

  /**
   * An ordered list of preferred languages based on:
   *
   *   1. App languages
   *   2. Web requested languages
   *   3. OS language
   *
   * @returns {string[]}
   */
  static getPreferredLanguages() {
    if (TranslationsParent.#preferredLanguages) {
      return TranslationsParent.#preferredLanguages;
    }

    if (!TranslationsParent.#observingLanguages) {
      Services.obs.addObserver(
        TranslationsParent.#resetPreferredLanguages,
        "intl:app-locales-changed"
      );
      Services.prefs.addObserver(
        "intl.accept_languages",
        TranslationsParent.#resetPreferredLanguages
      );
      TranslationsParent.#observingLanguages = true;
    }

    // The "Accept-Language" values that the localizer or user has indicated for
    // the preferences for the web. https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Accept-Language
    // Note that this preference often falls back ultimately to English, even if the
    // user doesn't actually speak English, or to other languages they do not speak.
    // However, this preference will be used as an indication that a user may prefer
    // this language.
    const webLanguages = Services.prefs
      .getComplexValue("intl.accept_languages", Ci.nsIPrefLocalizedString)
      .data.split(/\s*,\s*/g);

    // The system language could also be a good option for a language to offer the user.
    const osPrefs = Cc["@mozilla.org/intl/ospreferences;1"].getService(
      Ci.mozIOSPreferences
    );
    const systemLocales = this.mockedSystemLocales ?? osPrefs.systemLocales;

    // Combine the locales together.
    const preferredLocales = new Set([
      ...Services.locale.appLocalesAsBCP47,
      ...webLanguages,
      ...systemLocales,
    ]);

    // Attempt to convert the locales to lang tags. Do not completely trust the
    // values coming from preferences and the OS to have been validated as correct
    // BCP 47 locale identifiers.
    const langTags = new Set();
    for (const locale of preferredLocales) {
      try {
        langTags.add(new Intl.Locale(locale).language);
      } catch (_) {
        // The locale was invalid, discard it.
      }
    }

    // Convert the Set to an array to indicate that it is an ordered listing of languages.
    TranslationsParent.#preferredLanguages = [...langTags];

    return TranslationsParent.#preferredLanguages;
  }

  async receiveMessage({ name, data }) {
    switch (name) {
      case "Translations:GetTranslationsEnginePayload": {
        const { fromLanguage, toLanguage } = data;
        const bergamotWasmArrayBuffer = this.#getBergamotWasmArrayBuffer();

        let files = await this.getLanguageTranslationModelFiles(
          fromLanguage,
          toLanguage
        );

        let languageModelFiles;
        if (files) {
          languageModelFiles = [files];
        } else {
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
          languageModelFiles = [files1, files2];
        }

        return {
          bergamotWasmArrayBuffer: await bergamotWasmArrayBuffer,
          languageModelFiles,
          isMocked: TranslationsParent.#isTranslationsEngineMocked,
        };
      }
      case "Translations:GetLanguageIdEnginePayload": {
        const [modelBuffer, wasmBuffer] = await Promise.all([
          this.#getLanguageIdModelArrayBuffer(),
          this.#getLanguageIdWasmArrayBuffer(),
        ]);
        return {
          modelBuffer,
          wasmBuffer,
          mockedConfidence: TranslationsParent.#mockedLanguageIdConfidence,
          mockedLangTag: TranslationsParent.#mockedLangTag,
        };
      }
      case "Translations:GetIsTranslationsEngineMocked": {
        return TranslationsParent.#isTranslationsEngineMocked;
      }
      case "Translations:GetIsTranslationsEngineSupported": {
        return TranslationsParent.getIsTranslationsEngineSupported();
      }
      case "Translations:FullPageTranslationFailed": {
        this.languageState.error = data.reason;
        break;
      }
      case "Translations:GetSupportedLanguages": {
        return this.getSupportedLanguages();
      }
      case "Translations:HasAllFilesForLanguage": {
        return this.hasAllFilesForLanguage(data.language);
      }
      case "Translations:DownloadLanguageFiles": {
        return this.downloadLanguageFiles(data.language);
      }
      case "Translations:DownloadAllFiles": {
        return this.downloadAllFiles();
      }
      case "Translations:DeleteAllLanguageFiles": {
        return this.deleteAllLanguageFiles();
      }
      case "Translations:DeleteLanguageFiles": {
        return this.deleteLanguageFiles(data.language);
      }
      case "Translations:GetLanguagePairs": {
        return this.getLanguagePairs();
      }
      case "Translations:GetPreferredLanguages": {
        return TranslationsParent.getPreferredLanguages();
      }
      case "Translations:EngineIsReady": {
        this.isEngineReady = true;
        this.languageState.isEngineReady = true;
        break;
      }
      case "Translations:GetTranslationConditions": {
        const maybeAutoTranslate = TranslationsParent.#maybeAutoTranslate(
          data.docLangTag
        );
        const maybeNeverTranslate =
          TranslationsParent.shouldNeverTranslateLanguage(data.docLangTag) ||
          (await this.shouldNeverTranslateSite());

        if (maybeAutoTranslate && !maybeNeverTranslate) {
          this.languageState.requestedTranslationPair = {
            fromLanguage: data.docLangTag,
            toLanguage: data.userLangTag,
          };
        }

        return { maybeAutoTranslate, maybeNeverTranslate };
      }
      case "Translations:ReportDetectedLangTags": {
        this.languageState.detectedLanguages = data.langTags;
        return undefined;
      }
    }
    return undefined;
  }

  /**
   * Returns true if translations should auto-translate from the given
   * language, otherwise returns false.
   *
   * @param {string} langTag - A BCP-47 language tag
   * @returns {boolean}
   */
  static #maybeAutoTranslate(langTag) {
    if (
      // The user has not marked this language as always translate.
      !TranslationsParent.shouldAlwaysTranslateLanguage(langTag) &&
      // The pref to always auto-translate is off.
      !lazy.autoTranslatePagePref
    ) {
      return false;
    }

    if (TranslationsParent.#isPageRestoredForAutoTranslate) {
      // The user clicked the restore button. Respect it for one page load.
      TranslationsParent.#isPageRestoredForAutoTranslate = false;

      // Skip this auto-translation.
      return false;
    }

    // The page can be auto-translated
    return true;
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

    /** @type {LanguageIdModelRecord[]} */
    let modelRecords = await TranslationsParent.getMaxVersionRecords(client);

    if (modelRecords.length === 0) {
      throw new Error(
        "Unable to get language-identification model record from remote settings"
      );
    }

    if (modelRecords.length > 1) {
      TranslationsParent.reportError(
        new Error(
          "Expected the language-identification model collection to have only 1 record."
        ),
        modelRecords
      );
    }
    const [modelRecord] = modelRecords;

    await chaosMode(1 / 3);

    /** @type {{buffer: ArrayBuffer}} */
    const { buffer } = await client.attachments.download(modelRecord);

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
    if (TranslationsParent.#languageIdModelsRemoteClient) {
      return TranslationsParent.#languageIdModelsRemoteClient;
    }

    /** @type {RemoteSettingsClient} */
    const client = lazy.RemoteSettings("translations-identification-models");

    TranslationsParent.#languageIdModelsRemoteClient = client;
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
    let wasmRecords = await TranslationsParent.getMaxVersionRecords(client, {
      filters: { name: "fasttext-wasm" },
    });

    if (wasmRecords.length === 0) {
      // The remote settings client provides an empty list of records when there is
      // an error.
      throw new Error(
        'Unable to get "fasttext-wasm" language-identification wasm binary from Remote Settings.'
      );
    }

    if (wasmRecords.length > 1) {
      TranslationsParent.reportError(
        new Error(
          'Expected the "fasttext-wasm" language-identification wasm collection to only have 1 record.'
        ),
        wasmRecords
      );
    }

    // Unlike the models, greedily download the wasm. It will pull it from a locale
    // cache on disk if it's already been downloaded. Do not retain a copy, as
    // this will be running in the parent process. It's not worth holding onto
    // this much memory, so reload it every time it is needed.

    await chaosMode(1 / 3);

    /** @type {{buffer: ArrayBuffer}} */
    const { buffer } = await client.attachments.download(wasmRecords[0]);

    const duration = (Date.now() - start) / 1000;
    lazy.console.log(
      `Remote language-identification wasm binary loaded in ${duration} seconds.`
    );

    return buffer;
  }

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
   * Get the list of translation pairs supported by the translations engine.
   *
   * @returns {Promise<Array<LanguagePair>>}
   */
  async getLanguagePairs() {
    const records = await this.#getTranslationModelRecords();
    const languagePairMap = new Map();

    for (const { fromLang, toLang, version } of records.values()) {
      const isBeta = Services.vc.compare(version, "1.0") < 0;
      const key = TranslationsParent.languagePairKey(fromLang, toLang);
      if (!languagePairMap.has(key)) {
        languagePairMap.set(key, { fromLang, toLang, isBeta });
      }
    }

    return Array.from(languagePairMap.values());
  }

  /**
   * Returns all of the information needed to render dropdowns for translation
   * language selection.
   *
   * @returns {Promise<SupportedLanguages>}
   */
  async getSupportedLanguages() {
    const languagePairs = await this.getLanguagePairs();

    /** @type {Map<string, boolean>} */
    const fromLanguages = new Map();
    /** @type {Map<string, boolean>} */
    const toLanguages = new Map();

    for (const { fromLang, toLang, isBeta } of languagePairs) {
      // [BetaLanguage, BetaLanguage]       => isBeta == true,
      // [BetaLanguage, NonBetaLanguage]    => isBeta == true,
      // [NonBetaLanguage, BetaLanguage]    => isBeta == true,
      // [NonBetaLanguage, NonBetaLanguage] => isBeta == false,
      if (isBeta) {
        // If these languages are part of a beta languagePair, at least one of them is a beta language
        // but the other may not be, so only tentatively mark them as beta if there is no entry.
        if (!fromLanguages.has(fromLang)) {
          fromLanguages.set(fromLang, isBeta);
        }
        if (!toLanguages.has(toLang)) {
          toLanguages.set(toLang, isBeta);
        }
      } else {
        // If these languages are part of a non-beta languagePair, then they are both
        // guaranteed to be non-beta languages. Idempotently overwrite any previous entry.
        fromLanguages.set(fromLang, isBeta);
        toLanguages.set(toLang, isBeta);
      }
    }

    // Build a map of the langTag to the display name.
    /** @type {Map<string, string>} */
    const displayNames = new Map();
    {
      const dn = new Services.intl.DisplayNames(undefined, {
        type: "language",
      });

      for (const langTagSet of [fromLanguages, toLanguages]) {
        for (const langTag of langTagSet.keys()) {
          if (displayNames.has(langTag)) {
            continue;
          }
          displayNames.set(langTag, dn.of(langTag));
        }
      }
    }

    const addDisplayName = ([langTag, isBeta]) => ({
      langTag,
      isBeta,
      displayName: displayNames.get(langTag),
    });

    const sort = (a, b) => a.displayName.localeCompare(b.displayName);

    return {
      languagePairs,
      fromLanguages: Array.from(fromLanguages.entries())
        .map(addDisplayName)
        .sort(sort),
      toLanguages: Array.from(toLanguages.entries())
        .map(addDisplayName)
        .sort(sort),
    };
  }

  /**
   * Lazily initializes the RemoteSettingsClient for the language models.
   *
   * @returns {RemoteSettingsClient}
   */
  #getTranslationModelsRemoteClient() {
    if (TranslationsParent.#translationModelsRemoteClient) {
      return TranslationsParent.#translationModelsRemoteClient;
    }

    /** @type {RemoteSettingsClient} */
    const client = lazy.RemoteSettings("translations-models");
    TranslationsParent.#translationModelsRemoteClient = client;

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
   * Retrieves the maximum version of each record in the RemoteSettingsClient.
   *
   * If the client contains two different-version copies of the same record (e.g. 1.0 and 1.1)
   * then only the 1.1-version record will be returned in the resulting collection.
   *
   * @param {RemoteSettingsClient} remoteSettingsClient
   * @param {Object} [options]
   *   @param {Object} [options.filters={}]
   *     The filters to apply when retrieving the records from RemoteSettings.
   *     Filters should correspond to properties on the RemoteSettings records themselves.
   *     For example, A filter to retrieve only records with a `fromLang` value of "en" and a `toLang` value of "es":
   *     { filters: { fromLang: "en", toLang: "es" } }
   *   @param {Function} [options.lookupKey=(record => record.name)]
   *     The function to use to extract a lookup key from each record.
   *     This function should take a record as input and return a string that represents the lookup key for the record.
   *     For most record types, the name (default) is sufficient, however if a collection contains records with
   *     non-unique name values, it may be necessary to provide an alternative function here.
   * @returns {Array<TranslationModelRecord | LanguageIdModelRecord | WasmRecord>}
   */
  static async getMaxVersionRecords(
    remoteSettingsClient,
    { filters = {}, lookupKey = record => record.name } = {}
  ) {
    try {
      await chaosMode(1 / 4);
    } catch (_error) {
      // Simulate an error by providing empty records.
      return [];
    }
    const retrievedRecords = await remoteSettingsClient.get({
      // Pull the records from the network.
      syncIfEmpty: true,
      // Don't verify the signature if the client is mocked.
      verifySignature: VERIFY_SIGNATURES_FROM_FS,
      // Apply any filters for retrieving the records.
      filters,
    });

    // Create a mapping to only the max version of each record discriminated by
    // the result of the lookupKey() function.
    const maxVersionRecordMap = retrievedRecords.reduce((records, record) => {
      const key = lookupKey(record);
      const existing = records.get(key);
      if (
        !existing ||
        // existing version less than record version
        Services.vc.compare(existing.version, record.version) < 0
      ) {
        records.set(key, record);
      }
      return records;
    }, new Map());

    return Array.from(maxVersionRecordMap.values());
  }

  /**
   * Lazily initializes the model records, and returns the cached ones if they
   * were already retrieved. The key of the returned `Map` is the record id.
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
    const translationModelRecords =
      await TranslationsParent.getMaxVersionRecords(client, {
        // Names in this collection are not unique, so we are appending the languagePairKey
        // to guarantee uniqueness.
        lookupKey: record =>
          `${record.name}${TranslationsParent.languagePairKey(
            record.fromLang,
            record.toLang
          )}`,
      });

    if (translationModelRecords.length === 0) {
      throw new Error("Unable to retrieve the translation models.");
    }

    for (const record of TranslationsParent.ensureLanguagePairsHavePivots(
      translationModelRecords
    )) {
      this.#translationModelRecords.set(record.id, record);
    }

    const duration = (Date.now() - now) / 1000;
    lazy.console.log(
      `Remote language models loaded in ${duration} seconds.`,
      this.#translationModelRecords
    );

    return this.#translationModelRecords;
  }

  /**
   * This implementation assumes that every language pair has access to the
   * pivot language. If any languages are added without a pivot language, or the
   * pivot language is changed, then this implementation will need a more complicated
   * language solver. This means that any UI pickers would need to be updated, and
   * the pivot language selection would need a solver.
   *
   * @param {TranslationModelRecord[] | LanguagePair[]} records
   */
  static ensureLanguagePairsHavePivots(records) {
    // lang -> pivot
    const hasToPivot = new Set();
    // pivot -> en
    const hasFromPivot = new Set();

    const fromLangs = new Set();
    const toLangs = new Set();

    for (const { fromLang, toLang } of records) {
      fromLangs.add(fromLang);
      toLangs.add(toLang);

      if (toLang === PIVOT_LANGUAGE) {
        // lang -> pivot
        hasToPivot.add(fromLang);
      }
      if (fromLang === PIVOT_LANGUAGE) {
        // pivot -> en
        hasFromPivot.add(toLang);
      }
    }

    const fromLangsToRemove = new Set();
    const toLangsToRemove = new Set();

    for (const lang of fromLangs) {
      if (lang === PIVOT_LANGUAGE) {
        continue;
      }
      // Check for "lang -> pivot"
      if (!hasToPivot.has(lang)) {
        TranslationsParent.reportError(
          new Error(
            `The "from" language model "${lang}" is being discarded as it doesn't have a pivot language.`
          )
        );
        fromLangsToRemove.add(lang);
      }
    }

    for (const lang of toLangs) {
      if (lang === PIVOT_LANGUAGE) {
        continue;
      }
      // Check for "pivot -> lang"
      if (!hasFromPivot.has(lang)) {
        TranslationsParent.reportError(
          new Error(
            `The "to" language model "${lang}" is being discarded as it doesn't have a pivot language.`
          )
        );
        toLangsToRemove.add(lang);
      }
    }

    const after = records.filter(record => {
      if (fromLangsToRemove.has(record.fromLang)) {
        return false;
      }
      if (toLangsToRemove.has(record.toLang)) {
        return false;
      }
      return true;
    });
    return after;
  }

  /**
   * Lazily initializes the RemoteSettingsClient for the downloaded wasm binary data.
   *
   * @returns {RemoteSettingsClient}
   */
  #getTranslationsWasmRemoteClient() {
    if (TranslationsParent.#translationsWasmRemoteClient) {
      return TranslationsParent.#translationsWasmRemoteClient;
    }

    /** @type {RemoteSettingsClient} */
    const client = lazy.RemoteSettings("translations-wasm");

    TranslationsParent.#translationsWasmRemoteClient = client;

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
    const wasmRecords = await TranslationsParent.getMaxVersionRecords(client, {
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
      TranslationsParent.reportError(
        new Error("Expected the bergamot-translator to only have 1 record."),
        wasmRecords
      );
    }

    // Unlike the models, greedily download the wasm. It will pull it from a locale
    // cache on disk if it's already been downloaded. Do not retain a copy, as
    // this will be running in the parent process. It's not worth holding onto
    // this much memory, so reload it every time it is needed.

    await chaosModeError(1 / 3);

    /** @type {{buffer: ArrayBuffer}} */
    const { buffer } = await client.attachments.download(wasmRecords[0]);

    const duration = Date.now() - start;
    lazy.console.log(
      `"bergamot-translator" wasm binary loaded in ${duration / 1000} seconds`
    );

    return buffer;
  }

  /**
   * Deletes language files that match a language.
   *
   * @param {string} requestedLanguage The BCP 47 language tag.
   */
  async deleteLanguageFiles(language) {
    const client = this.#getTranslationModelsRemoteClient();
    const isForDeletion = true;
    return Promise.all(
      Array.from(
        await this.getRecordsForTranslatingToAndFromAppLanguage(
          language,
          isForDeletion
        )
      ).map(record => {
        lazy.console.log("Deleting record", record);
        return client.attachments.deleteDownloaded(record);
      })
    );
  }

  /**
   * Download language files that match a language.
   *
   * @param {string} requestedLanguage The BCP 47 language tag.
   */
  async downloadLanguageFiles(language) {
    const client = this.#getTranslationModelsRemoteClient();

    const queue = [];

    for (const record of await this.getRecordsForTranslatingToAndFromAppLanguage(
      language
    )) {
      const download = () => {
        lazy.console.log("Downloading record", record.name, record.id);
        return client.attachments.download(record);
      };
      queue.push({ download });
    }

    return downloadManager(queue);
  }

  /**
   * Download all files used for translations.
   */
  async downloadAllFiles() {
    const client = this.#getTranslationModelsRemoteClient();

    const queue = [];

    for (const [recordId, record] of await this.#getTranslationModelRecords()) {
      queue.push({
        onSuccess: () => {
          this.sendQuery("Translations:DownloadedLanguageFile", { recordId });
        },
        // The download may be attempted multiple times.
        onFailure: () => {
          this.sendQuery("Translations:DownloadLanguageFileError", {
            recordId,
          });
        },
        download: () => client.attachments.download(record),
      });
    }

    queue.push({ download: () => this.#getBergamotWasmArrayBuffer() });
    queue.push({ download: () => this.#getLanguageIdModelArrayBuffer() });
    queue.push({ download: () => this.#getLanguageIdWasmArrayBuffer() });

    return downloadManager(queue);
  }

  /**
   * Delete all language model files.
   * @returns {Promise<string[]>} A list of record IDs.
   */
  async deleteAllLanguageFiles() {
    const client = this.#getTranslationModelsRemoteClient();
    await chaosMode();
    await client.attachments.deleteAll();
    return [...(await this.#getTranslationModelRecords()).keys()];
  }

  /**
   * Only returns true if all language files are present for a requested language.
   * It's possible only half the files exist for a pivot translation into another
   * language, or there was a download error, and we're still missing some files.
   *
   * @param {string} requestedLanguage The BCP 47 language tag.
   */
  async hasAllFilesForLanguage(requestedLanguage) {
    const client = this.#getTranslationModelsRemoteClient();
    for (const record of await this.getRecordsForTranslatingToAndFromAppLanguage(
      requestedLanguage,
      true
    )) {
      if (!(await client.attachments.isDownloaded(record))) {
        return false;
      }
    }

    return true;
  }

  /**
   * Get the necessary files for translating to and from the app language and a
   * requested language. This may require the files for a pivot language translation
   * if there is no language model for a direct translation.
   *
   * @param {string} requestedLanguage The BCP 47 language tag.
   * @param {boolean} isForDeletion - Return a more restrictive set of languages, as
   *                  these files are marked for deletion. We don't want to remove
   *                  files that are needed for some other language's pivot translation.
   * @returns {Set<TranslationModelRecord>}
   */
  async getRecordsForTranslatingToAndFromAppLanguage(
    requestedLanguage,
    isForDeletion = false
  ) {
    const records = await this.#getTranslationModelRecords();
    const appLanguage = new Intl.Locale(Services.locale.appLocaleAsBCP47)
      .language;

    let matchedRecords = new Set();

    if (requestedLanguage === appLanguage) {
      // There are no records if the requested language and app language are the same.
      return matchedRecords;
    }

    const addLanguagePair = (fromLang, toLang) => {
      let matchFound = false;
      for (const record of records.values()) {
        if (record.fromLang === fromLang && record.toLang === toLang) {
          matchedRecords.add(record);
          matchFound = true;
        }
      }
      return matchFound;
    };

    if (
      // Is there a direct translation?
      !addLanguagePair(requestedLanguage, appLanguage)
    ) {
      // This is no direct translation, get the pivot files.
      addLanguagePair(requestedLanguage, PIVOT_LANGUAGE);
      // These files may be required for other pivot translations, so don't remove
      // them if we are deleting records.
      if (!isForDeletion) {
        addLanguagePair(PIVOT_LANGUAGE, appLanguage);
      }
    }

    if (
      // Is there a direct translation?
      !addLanguagePair(appLanguage, requestedLanguage)
    ) {
      // This is no direct translation, get the pivot files.
      addLanguagePair(PIVOT_LANGUAGE, requestedLanguage);
      // These files may be required for other pivot translations, so don't remove
      // them if we are deleting records.
      if (!isForDeletion) {
        addLanguagePair(appLanguage, PIVOT_LANGUAGE);
      }
    }

    return matchedRecords;
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

        await chaosMode(1 / 3);

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
   * @param {null | RemoteSettingsClient} [translationModelsRemoteClient]
   * @param {null | RemoteSettingsClient} [translationsWasmRemoteClient]
   */
  static mockTranslationsEngine(
    translationModelsRemoteClient,
    translationsWasmRemoteClient
  ) {
    lazy.console.log("Mocking RemoteSettings for the translations engine.");
    TranslationsParent.#translationModelsRemoteClient =
      translationModelsRemoteClient;
    TranslationsParent.#translationsWasmRemoteClient =
      translationsWasmRemoteClient;
    TranslationsParent.#isTranslationsEngineMocked = true;
  }

  /**
   * Remove the mocks.
   */
  static unmockTranslationsEngine() {
    lazy.console.log(
      "Removing RemoteSettings mock for the translations engine."
    );
    TranslationsParent.#translationModelsRemoteClient = null;
    TranslationsParent.#translationsWasmRemoteClient = null;
    TranslationsParent.#isTranslationsEngineMocked = false;
  }

  /**
   * For testing purposes, allow the LanguageIdEngine to be mocked. If called
   * with `null` in each argument, the mock is removed.
   *
   * @param {string} langTag - The BCP 47 language tag.
   * @param {number} confidence  - The confidence score of the detected language.
   * @param {RemoteSettingsClient} client
   */
  static mockLanguageIdentification(langTag, confidence, client) {
    lazy.console.log("Mocking language identification.", {
      langTag,
      confidence,
    });
    TranslationsParent.#mockedLangTag = langTag;
    TranslationsParent.#mockedLanguageIdConfidence = confidence;
    TranslationsParent.#languageIdModelsRemoteClient = client;
  }

  /**
   * Remove the mocks
   */
  static unmockLanguageIdentification() {
    lazy.console.log("Removing language identification mock.");
    TranslationsParent.#mockedLangTag = null;
    TranslationsParent.#mockedLanguageIdConfidence = null;
    TranslationsParent.#languageIdModelsRemoteClient = null;
  }
  /**
   * Report an error. Having this as a method allows tests to check that an error
   * was properly reported.
   * @param {Error} error - Providing an Error object makes sure the stack is properly
   *                        reported.
   * @param {any[]} args - Any args to pass on to console.error.
   */
  static reportError(error, ...args) {
    lazy.console.log(error, ...args);
  }

  /**
   * @param {string} fromLanguage
   * @param {string} toLanguage
   */
  translate(fromLanguage, toLanguage) {
    if (this.languageState.requestedTranslationPair) {
      // This page has already been translated, restore it and translate it
      // again once the actor has been recreated.
      TranslationsParent.#translateOnPageReload = { fromLanguage, toLanguage };
      this.restorePage(fromLanguage);
    } else {
      this.languageState.requestedTranslationPair = {
        fromLanguage,
        toLanguage,
      };
      this.sendAsyncMessage("Translations:TranslatePage", {
        fromLanguage,
        toLanguage,
      });
    }
  }

  /**
   * Restore the page to the original language by doing a hard reload.
   *
   * @param {string} fromLanguage A BCP-47 language tag
   */
  restorePage(fromLanguage) {
    if (
      lazy.autoTranslatePagePref ||
      TranslationsParent.shouldAlwaysTranslateLanguage(fromLanguage)
    ) {
      // Skip auto-translate for one page load.
      TranslationsParent.#isPageRestoredForAutoTranslate = true;
    }
    this.languageState.requestedTranslationPair = null;

    const browser = this.browsingContext.embedderElement;
    browser.reload();
  }

  /**
   * Keep track of when the location changes.
   */
  static #locationChangeId = 0;

  static onLocationChange(browser) {
    if (!lazy.translationsEnabledPref) {
      // The pref isn't enabled, so don't attempt to get the actor.
      return;
    }
    let windowGlobal = browser.browsingContext.currentWindowGlobal;
    let actor = windowGlobal.getActor("Translations");
    TranslationsParent.#locationChangeId++;
    actor.languageState.locationChangeId = TranslationsParent.#locationChangeId;
  }

  /**
   * Is this actor active for the current location change?
   *
   * @param {number} locationChangeId - The id sent by the "TranslationsParent:LanguageState" event.
   * @returns {boolean}
   */
  static isActiveLocation(locationChangeId) {
    return locationChangeId === TranslationsParent.#locationChangeId;
  }

  /**
   * Returns the lang tags that should be offered for translation.
   *
   * @returns {Promise<LangTags>}
   */
  getLangTagsForTranslation() {
    return this.sendQuery("Translations:GetLangTagsForTranslation");
  }

  /**
   * Returns the principal from the content window's origin.
   * @returns {nsIPrincipal}
   */
  getContentWindowPrincipal() {
    return this.sendQuery("Translations:GetContentWindowPrincipal");
  }

  /**
   * Returns true if the given language tag is present in the always-translate
   * languages preference, otherwise false.
   *
   * @param {string} langTag - A BCP-47 language tag
   * @returns {boolean}
   */
  static shouldAlwaysTranslateLanguage(langTag) {
    return lazy.alwaysTranslateLangTags.includes(langTag);
  }

  /**
   * Returns true if the given language tag is present in the never-translate
   * languages preference, otherwise false.
   *
   * @param {string} langTag - A BCP-47 language tag
   * @returns {boolean}
   */
  static shouldNeverTranslateLanguage(langTag) {
    return lazy.neverTranslateLangTags.includes(langTag);
  }

  /**
   * Returns true if the current site is denied permissions to translate,
   * otherwise returns false.
   *
   * @returns {Promise<boolean>}
   */
  async shouldNeverTranslateSite() {
    let principal;
    try {
      principal = await this.getContentWindowPrincipal();
    } catch {
      // Unable to get content window principal.
      return false;
    }
    const perms = Services.perms;
    const permission = perms.getPermissionObject(
      principal,
      TRANSLATIONS_PERMISSION,
      /* exactHost */ false
    );
    return permission?.capability === perms.DENY_ACTION;
  }

  /**
   * Removes the given language tag from the given preference.
   *
   * @param {string} langTag - A BCP-47 language tag
   * @param {string} prefName - The pref name
   */
  static #removeLangTagFromPref(langTag, prefName) {
    const langTags =
      prefName === ALWAYS_TRANSLATE_LANGS_PREF
        ? lazy.alwaysTranslateLangTags
        : lazy.neverTranslateLangTags;
    const newLangTags = langTags.filter(tag => tag !== langTag);
    Services.prefs.setCharPref(prefName, newLangTags.join(","));
  }

  /**
   * Adds the given language tag to the given preference.
   *
   * @param {string} langTag - A BCP-47 language tag
   * @param {string} prefName - The pref name
   */
  static #addLangTagToPref(langTag, prefName) {
    const langTags =
      prefName === ALWAYS_TRANSLATE_LANGS_PREF
        ? lazy.alwaysTranslateLangTags
        : lazy.neverTranslateLangTags;
    if (!langTags.includes(langTag)) {
      langTags.push(langTag);
    }
    Services.prefs.setCharPref(prefName, langTags.join(","));
  }

  /**
   * Toggles the always-translate language preference by adding the language
   * to the pref list if it is not present, or removing it if it is present.
   *
   * @param {string} langTag - A BCP-47 language tag
   */
  static toggleAlwaysTranslateLanguagePref(langTag) {
    if (TranslationsParent.shouldAlwaysTranslateLanguage(langTag)) {
      // The pref was toggled off for this langTag
      this.#removeLangTagFromPref(langTag, ALWAYS_TRANSLATE_LANGS_PREF);
      return;
    }

    // The pref was toggled on for this langTag
    this.#addLangTagToPref(langTag, ALWAYS_TRANSLATE_LANGS_PREF);
    this.#removeLangTagFromPref(langTag, NEVER_TRANSLATE_LANGS_PREF);
  }

  /**
   * Toggles the never-translate language preference by adding the language
   * to the pref list if it is not present, or removing it if it is present.
   *
   * @param {string} langTag - A BCP-47 language tag
   */
  static toggleNeverTranslateLanguagePref(langTag) {
    if (TranslationsParent.shouldNeverTranslateLanguage(langTag)) {
      // The pref was toggled off for this langTag
      this.#removeLangTagFromPref(langTag, NEVER_TRANSLATE_LANGS_PREF);
      return;
    }

    // The pref was toggled on for this langTag
    this.#addLangTagToPref(langTag, NEVER_TRANSLATE_LANGS_PREF);
    this.#removeLangTagFromPref(langTag, ALWAYS_TRANSLATE_LANGS_PREF);
  }

  /**
   * Toggles the never-translate site permissions by adding DENY_ACTION to
   * the site principal if it is not present, or removing it if it is present.
   */
  async toggleNeverTranslateSitePermissions() {
    const perms = Services.perms;
    const principal = await this.getContentWindowPrincipal();
    const shouldNeverTranslateSite = await this.shouldNeverTranslateSite();
    if (shouldNeverTranslateSite) {
      perms.removeFromPrincipal(principal, TRANSLATIONS_PERMISSION);
    } else {
      perms.addFromPrincipal(
        principal,
        TRANSLATIONS_PERMISSION,
        perms.DENY_ACTION
      );
    }
  }
}

/**
 * WebAssembly modules must be instantiated from a Worker, since it's considered
 * unsafe eval.
 */
function detectSimdSupport() {
  return new Promise(resolve => {
    lazy.console.log("Loading wasm simd detector worker.");

    const worker = new Worker(
      "chrome://global/content/translations/simd-detect-worker.js"
    );

    // This should pretty much immediately resolve, so it does not need Firefox shutdown
    // detection.
    worker.addEventListener("message", ({ data }) => {
      resolve(data.isSimdSupported);
      worker.terminate();
    });
  });
}

/**
 * State that affects the UI. Any of the state that gets set triggers a dispatch to update
 * the UI.
 */
class TranslationsLanguageState {
  /**
   * @param {TranslationsParent} actor
   */
  constructor(actor) {
    this.#actor = actor;
    this.dispatch();
  }

  /**
   * The data members for TranslationsLanguageState, see the getters for their
   * documentation.
   */

  /** @type {TranslationsParent} */
  #actor;

  /** @type {TranslationPair | null} */
  #requestedTranslationPair = null;

  /** @type {LangTags | null} */
  #detectedLanguages = null;

  /** @type {number} */
  #locationChangeId = -1;

  /** @type {null | TranslationErrors} */
  #error = null;

  #isEngineReady = false;

  /**
   * Dispatch anytime the language details change, so that any UI can react to it.
   */
  dispatch() {
    if (!TranslationsParent.isActiveLocation(this.#locationChangeId)) {
      // Do not dispatch as this location is not active.
      return;
    }

    const browser = this.#actor.browsingContext.top.embedderElement;
    if (!browser) {
      return;
    }
    const { CustomEvent } = browser.ownerGlobal;
    browser.dispatchEvent(
      new CustomEvent("TranslationsParent:LanguageState", {
        bubbles: true,
        detail: {
          detectedLanguages: this.#detectedLanguages,
          requestedTranslationPair: this.#requestedTranslationPair,
          error: this.#error,
          isEngineReady: this.#isEngineReady,
        },
      })
    );
  }

  /**
   * When a translation is requested, this contains the translation pair. This means
   * that the TranslationsChild should be creating a TranslationsDocument and keep
   * the page updated with the target language.
   *
   * @returns {TranslationPair | null}
   */
  get requestedTranslationPair() {
    return this.#requestedTranslationPair;
  }

  set requestedTranslationPair(requestedTranslationPair) {
    this.#error = null;
    this.#isEngineReady = false;
    this.#requestedTranslationPair = requestedTranslationPair;
    this.dispatch();
  }

  /**
   * The TranslationsChild will detect languages and offer them up for translation.
   * The results are stored here.
   *
   * @returns {LangTags | null}
   */
  get detectedLanguages() {
    return this.#detectedLanguages;
  }

  set detectedLanguages(detectedLanguages) {
    this.#detectedLanguages = detectedLanguages;
    this.dispatch();
  }

  /**
   * This id represents the last location change that happened for this actor. This
   * allows the UI to disambiguate when there are races and out of order events that
   * are dispatched. Only the most up to date `locationChangeId` is used.
   *
   * @returns {number}
   */
  get locationChangeId() {
    return this.#locationChangeId;
  }

  set locationChangeId(locationChangeId) {
    this.#locationChangeId = locationChangeId;

    // When the location changes remove the previous error.
    this.#error = null;

    this.dispatch();
  }

  /**
   * The last error that occured during translation.
   */
  get error() {
    return this.#error;
  }

  set error(error) {
    this.#error = error;
    // Setting an error invalidates the requested translation pair.
    this.#requestedTranslationPair = null;
    this.#isEngineReady = false;
    this.dispatch();
  }

  /**
   * Stores when the translations engine is ready. The wasm and language files must
   * be downloaded, which can take some time.
   */
  get isEngineReady() {
    return this.#isEngineReady;
  }

  set isEngineReady(isEngineReady) {
    this.#isEngineReady = isEngineReady;
    this.dispatch();
  }
}

/**
 * @typedef {Object} QueueItem
 * @prop {Function} download
 * @prop {Function} [onSuccess]
 * @prop {Function} [onFailure]
 * @prop {number} [retriesLeft]
 */

/**
 * Manage the download of the files by providing a maximum number of concurrent files
 * and the ability to retry a file download in case of an error.
 *
 * @param {QueueItem[]} queue
 */
async function downloadManager(queue) {
  const NOOP = () => {};

  const pendingDownloadAttempts = new Set();
  let failCount = 0;
  let index = 0;
  const start = Date.now();
  const originalQueueLength = queue.length;

  while (index < queue.length || pendingDownloadAttempts.size > 0) {
    // Start new downloads up to the maximum limit
    while (
      index < queue.length &&
      pendingDownloadAttempts.size < TranslationsParent.MAX_CONCURRENT_DOWNLOADS
    ) {
      lazy.console.log(`Starting download ${index + 1} of ${queue.length}`);

      const {
        download,
        onSuccess = NOOP,
        onFailure = NOOP,
        retriesLeft = TranslationsParent.MAX_DOWNLOAD_RETRIES,
      } = queue[index];

      const handleFailedDownload = error => {
        // The download failed. Either retry it, or report the failure.
        TranslationsParent.reportError(
          new Error("Failed to download file."),
          error
        );

        const newRetriesLeft = retriesLeft - 1;

        if (retriesLeft > 0) {
          lazy.console.log(
            `Queueing another attempt. ${newRetriesLeft} attempts left.`
          );
          queue.push({
            download,
            retriesLeft: newRetriesLeft,
            onSuccess,
            onFailure,
          });
        } else {
          // Give up on this download.
          failCount++;
          onFailure();
        }
      };

      const afterDownloadAttempt = () => {
        pendingDownloadAttempts.delete(downloadAttempt);
      };

      // Kick off the download. If it fails, retry it a certain number of attempts.
      // This is done asynchronously from the rest of the for loop.
      const downloadAttempt = download()
        .then(onSuccess, handleFailedDownload)
        .then(afterDownloadAttempt);

      pendingDownloadAttempts.add(downloadAttempt);
      index++;
    }

    // Wait for any active downloads to complete.
    await Promise.race(pendingDownloadAttempts);
  }

  const duration = ((Date.now() - start) / 1000).toFixed(3);

  if (failCount > 0) {
    const message = `Finished downloads in ${duration} seconds, but ${failCount} download(s) failed.`;
    lazy.console.log(
      `Finished downloads in ${duration} seconds, but ${failCount} download(s) failed.`
    );
    throw new Error(message);
  }

  lazy.console.log(
    `Finished ${originalQueueLength} downloads in ${duration} seconds.`
  );
}

/**
 * The translations code has lots of async code and fallible network requests. To test
 * this manually while using the feature, enable chaos mode by setting "errors" to true
 * and "timeoutMS" to a positive number of milliseconds.
 * prefs to true:
 *
 *  - browser.translations.chaos.timeoutMS
 *  - browser.translations.chaos.errors
 */
async function chaosMode(probability = 0.5) {
  await chaosModeTimer();
  await chaosModeError(probability);
}

/**
 * The translations code has lots of async code that relies on the network. To test
 * this manually while using the feature, enable chaos mode by setting the following pref
 * to a positive number of milliseconds.
 *
 *  - browser.translations.chaos.timeoutMS
 */
async function chaosModeTimer() {
  if (lazy.chaosTimeoutMSPref) {
    const timeout = Math.random() * lazy.chaosTimeoutMSPref;
    lazy.console.log(
      `Chaos mode timer started for ${(timeout / 1000).toFixed(1)} seconds.`
    );
    await new Promise(resolve => lazy.setTimeout(resolve, timeout));
  }
}

/**
 * The translations code has lots of async code that is fallible. To test this manually
 * while using the feature, enable chaos mode by setting the following pref to true.
 *
 *  - browser.translations.chaos.errors
 */
async function chaosModeError(probability = 0.5) {
  if (lazy.chaosErrorsPref && Math.random() < probability) {
    lazy.console.trace(`Chaos mode error generated.`);
    throw new Error(
      `Chaos Mode error from the pref "browser.translations.chaos.errors".`
    );
  }
}
