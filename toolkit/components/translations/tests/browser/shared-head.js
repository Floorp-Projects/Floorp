/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Avoid about:blank's non-standard behavior.
const BLANK_PAGE =
  "data:text/html;charset=utf-8,<!DOCTYPE html><title>Blank</title>Blank page";

const URL_COM_PREFIX = "https://example.com/browser/";
const URL_ORG_PREFIX = "https://example.org/browser/";
const CHROME_URL_PREFIX = "chrome://mochitests/content/browser/";
const DIR_PATH = "toolkit/components/translations/tests/browser/";
const TRANSLATIONS_TESTER_EN =
  URL_COM_PREFIX + DIR_PATH + "translations-tester-en.html";
const TRANSLATIONS_TESTER_ES =
  URL_COM_PREFIX + DIR_PATH + "translations-tester-es.html";
const TRANSLATIONS_TESTER_FR =
  URL_COM_PREFIX + DIR_PATH + "translations-tester-fr.html";
const TRANSLATIONS_TESTER_ES_2 =
  URL_COM_PREFIX + DIR_PATH + "translations-tester-es-2.html";
const TRANSLATIONS_TESTER_ES_DOT_ORG =
  URL_ORG_PREFIX + DIR_PATH + "translations-tester-es.html";
const TRANSLATIONS_TESTER_NO_TAG =
  URL_COM_PREFIX + DIR_PATH + "translations-tester-no-tag.html";

/**
 * The mochitest runs in the parent process. This function opens up a new tab,
 * opens up about:translations, and passes the test requirements into the content process.
 *
 * @template T
 *
 * @param {object} options
 *
 * @param {T} options.dataForContent
 * The data must support structural cloning and will be passed into the
 * content process.
 *
 * @param {(args: { dataForContent: T, selectors: Record<string, string> }) => Promise<void>} options.runInPage
 * This function must not capture any values, as it will be cloned in the content process.
 * Any required data should be passed in using the "dataForContent" parameter. The
 * "selectors" property contains any useful selectors for the content.
 *
 * @param {boolean} [options.disabled]
 * Disable the panel through a pref.
 *
 * @param {number} detectedLanguageConfidence
 * This is the value for the MockedLanguageIdEngine to give as a confidence score for
 * the mocked detected language.
 *
 * @param {string} detectedLangTag
 * This is the BCP 47 language tag for the MockedLanguageIdEngine to return as
 * the mocked detected language.
 *
 * @param {Array<{ fromLang: string, toLang: string }>} options.languagePairs
 * The translation languages pairs to mock for the test.
 *
 * @param {Array<[string, string]>} options.prefs
 * Prefs to push on for the test.
 */
async function openAboutTranslations({
  dataForContent,
  disabled,
  runInPage,
  detectedLanguageConfidence,
  detectedLangTag,
  languagePairs = DEFAULT_LANGUAGE_PAIRS,
  prefs,
}) {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Enabled by default.
      ["browser.translations.enable", !disabled],
      ["browser.translations.logLevel", "All"],
      ...(prefs ?? []),
    ],
  });

  /**
   * Collect any relevant selectors for the page here.
   */
  const selectors = {
    pageHeader: '[data-l10n-id="about-translations-header"]',
    fromLanguageSelect: "select#language-from",
    toLanguageSelect: "select#language-to",
    translationTextarea: "textarea#translation-from",
    translationResult: "#translation-to",
    translationResultBlank: "#translation-to-blank",
    translationInfo: "#translation-info",
    noSupportMessage: "[data-l10n-id='about-translations-no-support']",
  };

  // Start the tab at a blank page.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    BLANK_PAGE,
    true // waitForLoad
  );

  const { removeMocks, remoteClients } = await createAndMockRemoteSettings({
    languagePairs,
    // TODO(Bug 1814168) - Do not test download behavior as this is not robustly
    // handled for about:translations yet.
    autoDownloadFromRemoteSettings: true,
    detectedLangTag,
    detectedLanguageConfidence,
  });

  // Now load the about:translations page, since the actor could be mocked.
  BrowserTestUtils.startLoadingURIString(
    tab.linkedBrowser,
    "about:translations"
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  // Resolve the files.
  await remoteClients.languageIdModels.resolvePendingDownloads(1);
  // The language id and translation engine each have a wasm file, so expect 2 downloads.
  await remoteClients.translationsWasm.resolvePendingDownloads(2);
  await remoteClients.translationModels.resolvePendingDownloads(
    languagePairs.length * FILES_PER_LANGUAGE_PAIR
  );

  await ContentTask.spawn(
    tab.linkedBrowser,
    { dataForContent, selectors },
    runInPage
  );

  await removeMocks();

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
}

/**
 * Naively prettify's html based on the opening and closing tags. This is not robust
 * for general usage, but should be adequate for these tests.
 * @param {string} html
 * @returns {string}
 */
function naivelyPrettify(html) {
  let result = "";
  let indent = 0;

  function addText(actualEndIndex) {
    const text = html.slice(startIndex, actualEndIndex).trim();
    if (text) {
      for (let i = 0; i < indent; i++) {
        result += "  ";
      }
      result += text + "\n";
    }
    startIndex = actualEndIndex;
  }

  let startIndex = 0;
  let endIndex = 0;
  for (; endIndex < html.length; endIndex++) {
    if (
      html[endIndex] === " " ||
      html[endIndex] === "\t" ||
      html[endIndex] === "n"
    ) {
      // Skip whitespace.
      // "   <div>foobar</div>"
      //  ^^^
      startIndex = endIndex;
      continue;
    }

    // Find all of the text.
    // "<div>foobar</div>"
    //       ^^^^^^
    while (endIndex < html.length && html[endIndex] !== "<") {
      endIndex++;
    }

    addText(endIndex);

    if (html[endIndex] === "<") {
      if (html[endIndex + 1] === "/") {
        // "<div>foobar</div>"
        //             ^
        while (endIndex < html.length && html[endIndex] !== ">") {
          endIndex++;
        }
        indent--;
        addText(endIndex + 1);
      } else {
        // "<div>foobar</div>"
        //  ^
        while (endIndex < html.length && html[endIndex] !== ">") {
          endIndex++;
        }
        // "<div>foobar</div>"
        //      ^
        addText(endIndex + 1);
        indent++;
      }
    }
  }

  return result.trim();
}

/**
 * This fake translator reports on the batching of calls by replacing the text
 * with a letter. Each call of the function moves the letter forward alphabetically.
 *
 * So consecutive calls would transform things like:
 *   "First translation" -> "aaaa aaaaaaaaa"
 *   "Second translation" -> "bbbbb bbbbbbbbb"
 *   "Third translation" -> "cccc ccccccccc"
 *
 * This can visually show what the translation batching behavior looks like.
 */
function createBatchFakeTranslator() {
  let letter = "a";
  /**
   * @param {string} message
   */
  return async function fakeTranslator(message) {
    /**
     * @param {Node} node
     */
    function transformNode(node) {
      if (typeof node.nodeValue === "string") {
        node.nodeValue = node.nodeValue.replace(/\w/g, letter);
      }
      for (const childNode of node.childNodes) {
        transformNode(childNode);
      }
    }

    const parser = new DOMParser();
    const translatedDoc = parser.parseFromString(message, "text/html");
    transformNode(translatedDoc.body);

    // "Increment" the letter.
    letter = String.fromCodePoint(letter.codePointAt(0) + 1);

    return [translatedDoc.body.innerHTML];
  };
}

/**
 * This fake translator reorders Nodes to be in alphabetical order, and then
 * uppercases the text. This allows for testing the reordering behavior of the
 * translation engine.
 *
 * @param {string} message
 */
async function reorderingTranslator(message) {
  /**
   * @param {Node} node
   */
  function transformNode(node) {
    if (typeof node.nodeValue === "string") {
      node.nodeValue = node.nodeValue.toUpperCase();
    }
    const nodes = [...node.childNodes];
    nodes.sort((a, b) =>
      (a.textContent?.trim() ?? "").localeCompare(b.textContent?.trim() ?? "")
    );
    for (const childNode of nodes) {
      childNode.remove();
    }
    for (const childNode of nodes) {
      // Re-append in sorted order.
      node.appendChild(childNode);
      transformNode(childNode);
    }
  }

  const parser = new DOMParser();
  const translatedDoc = parser.parseFromString(message, "text/html");
  transformNode(translatedDoc.body);

  return [translatedDoc.body.innerHTML];
}

/**
 * @returns {import("../../actors/TranslationsParent.sys.mjs").TranslationsParent}
 */
function getTranslationsParent() {
  return gBrowser.selectedBrowser.browsingContext.currentWindowGlobal.getActor(
    "Translations"
  );
}

/**
 * Closes the translations panel settings menu if it is open.
 */
function closeSettingsMenuIfOpen() {
  return waitForCondition(async () => {
    const settings = document.getElementById(
      "translations-panel-settings-menupopup"
    );
    if (!settings) {
      return true;
    }
    if (settings.state === "closed") {
      return true;
    }
    let popuphiddenPromise = BrowserTestUtils.waitForEvent(
      settings,
      "popuphidden"
    );
    PanelMultiView.hidePopup(settings);
    await popuphiddenPromise;
    return false;
  });
}

/**
 * Closes the translations panel if it is open.
 */
async function closeTranslationsPanelIfOpen() {
  await closeSettingsMenuIfOpen();
  return waitForCondition(async () => {
    const panel = document.getElementById("translations-panel");
    if (!panel) {
      return true;
    }
    if (panel.state === "closed") {
      return true;
    }
    let popuphiddenPromise = BrowserTestUtils.waitForEvent(
      panel,
      "popuphidden"
    );
    PanelMultiView.hidePopup(panel);
    await popuphiddenPromise;
    return false;
  });
}

/**
 * This is for tests that don't need a browser page to run.
 */
async function setupActorTest({
  languagePairs,
  prefs,
  detectedLanguageConfidence,
  detectedLangTag,
  autoDownloadFromRemoteSettings = false,
}) {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Enabled by default.
      ["browser.translations.enable", true],
      ["browser.translations.logLevel", "All"],
      ...(prefs ?? []),
    ],
  });

  const { remoteClients, removeMocks } = await createAndMockRemoteSettings({
    languagePairs,
    detectedLangTag,
    detectedLanguageConfidence,
    autoDownloadFromRemoteSettings,
  });

  // Create a new tab so each test gets a new actor, and doesn't re-use the old one.
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TRANSLATIONS_TESTER_EN,
    true // waitForLoad
  );

  return {
    actor: getTranslationsParent(),
    remoteClients,
    async cleanup() {
      await closeTranslationsPanelIfOpen();
      BrowserTestUtils.removeTab(tab);
      await removeMocks();
      TestTranslationsTelemetry.reset();
      return SpecialPowers.popPrefEnv();
    },
  };
}

/**
 * Provide some default language pairs when none are provided.
 */
const DEFAULT_LANGUAGE_PAIRS = [
  { fromLang: "en", toLang: "es" },
  { fromLang: "es", toLang: "en" },
];

async function createAndMockRemoteSettings({
  languagePairs = DEFAULT_LANGUAGE_PAIRS,
  detectedLanguageConfidence = 0.5,
  detectedLangTag = "en",
  autoDownloadFromRemoteSettings = false,
}) {
  const remoteClients = {
    translationModels: await createTranslationModelsRemoteClient(
      autoDownloadFromRemoteSettings,
      languagePairs
    ),
    translationsWasm: await createTranslationsWasmRemoteClient(
      autoDownloadFromRemoteSettings
    ),
    languageIdModels: await createLanguageIdModelsRemoteClient(
      autoDownloadFromRemoteSettings
    ),
  };

  // The TranslationsParent will pull the language pair values from the JSON dump
  // of Remote Settings. Clear these before mocking the translations engine.
  TranslationsParent.clearCache();

  TranslationsParent.mockTranslationsEngine(
    remoteClients.translationModels.client,
    remoteClients.translationsWasm.client
  );

  TranslationsParent.mockLanguageIdentification(
    detectedLangTag,
    detectedLanguageConfidence,
    remoteClients.languageIdModels.client
  );
  return {
    async removeMocks() {
      await remoteClients.translationModels.client.attachments.deleteAll();
      await remoteClients.translationsWasm.client.attachments.deleteAll();
      await remoteClients.languageIdModels.client.attachments.deleteAll();

      await remoteClients.translationModels.client.db.clear();
      await remoteClients.translationsWasm.client.db.clear();
      await remoteClients.languageIdModels.client.db.clear();

      TranslationsParent.unmockTranslationsEngine();
      TranslationsParent.unmockLanguageIdentification();
      TranslationsParent.clearCache();
    },
    remoteClients,
  };
}

async function loadTestPage({
  languagePairs,
  autoDownloadFromRemoteSettings = false,
  detectedLanguageConfidence,
  detectedLangTag,
  page,
  prefs,
  autoOffer,
  permissionsUrls = [],
}) {
  info(`Loading test page starting at url: ${page}`);
  Services.fog.testResetFOG();
  await SpecialPowers.pushPrefEnv({
    set: [
      // Enabled by default.
      ["browser.translations.enable", true],
      ["browser.translations.logLevel", "All"],
      ["browser.translations.panelShown", true],
      ["browser.translations.automaticallyPopup", true],
      ...(prefs ?? []),
    ],
  });
  await SpecialPowers.pushPermissions(
    permissionsUrls.map(url => ({
      type: "translations",
      allow: true,
      context: url,
    }))
  );

  if (autoOffer) {
    TranslationsParent.testAutomaticPopup = true;
  }

  // Start the tab at a blank page.
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    BLANK_PAGE,
    true // waitForLoad
  );

  const { remoteClients, removeMocks } = await createAndMockRemoteSettings({
    languagePairs,
    detectedLanguageConfidence,
    detectedLangTag,
    autoDownloadFromRemoteSettings,
  });

  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, page);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  if (autoOffer && TranslationsParent.shouldAlwaysOfferTranslations()) {
    info("Waiting for the popup to be automatically shown.");
    await waitForCondition(() => {
      const panel = document.getElementById("translations-panel");
      return panel && panel.state === "open";
    });
  }

  return {
    tab,
    remoteClients,

    /**
     * @param {number} count - Count of the language pairs expected.
     */
    async resolveDownloads(count) {
      await remoteClients.translationsWasm.resolvePendingDownloads(1);
      await remoteClients.translationModels.resolvePendingDownloads(
        FILES_PER_LANGUAGE_PAIR * count
      );
    },

    /**
     * @param {number} count - Count of the language pairs expected.
     */
    async rejectDownloads(count) {
      await remoteClients.translationsWasm.rejectPendingDownloads(1);
      await remoteClients.translationModels.rejectPendingDownloads(
        FILES_PER_LANGUAGE_PAIR * count
      );
    },

    async resolveLanguageIdDownloads() {
      await remoteClients.translationsWasm.resolvePendingDownloads(1);
      await remoteClients.languageIdModels.resolvePendingDownloads(1);
    },

    /**
     * @returns {Promise<void>}
     */
    async cleanup() {
      await closeTranslationsPanelIfOpen();
      await removeMocks();
      Services.fog.testResetFOG();
      TranslationsParent.testAutomaticPopup = false;
      TranslationsParent.resetHostsOffered();
      BrowserTestUtils.removeTab(tab);
      TestTranslationsTelemetry.reset();
      return Promise.all([
        SpecialPowers.popPrefEnv(),
        SpecialPowers.popPermissions(),
      ]);
    },

    /**
     * Runs a callback in the content page. The function's contents are serialized as
     * a string, and run in the page. The `translations-test.mjs` module is made
     * available to the page.
     *
     * @param {(TranslationsTest: import("./translations-test.mjs")) => any} callback
     * @returns {Promise<void>}
     */
    runInPage(callback, data = {}) {
      // ContentTask.spawn runs the `Function.prototype.toString` on this function in
      // order to send it into the content process. The following function is doing its
      // own string manipulation in order to load in the TranslationsTest module.
      const fn = new Function(/* js */ `
        const TranslationsTest = ChromeUtils.importESModule(
          "chrome://mochitests/content/browser/toolkit/components/translations/tests/browser/translations-test.mjs"
        );

        // Pass in the values that get injected by the task runner.
        TranslationsTest.setup({Assert, ContentTaskUtils, content});

        const data = ${JSON.stringify(data)};

        return (${callback.toString()})(TranslationsTest, data);
      `);

      return ContentTask.spawn(
        tab.linkedBrowser,
        {}, // Data to inject.
        fn
      );
    },
  };
}

/**
 * Captures any reported errors in the TranslationsParent.
 *
 * @param {Function} callback
 * @returns {Array<{ error: Error, args: any[] }>}
 */
async function captureTranslationsError(callback) {
  const { reportError } = TranslationsParent;

  let errors = [];
  TranslationsParent.reportError = (error, ...args) => {
    errors.push({ error, args });
  };

  await callback();

  // Restore the original function.
  TranslationsParent.reportError = reportError;
  return errors;
}

/**
 * Load a test page and run
 * @param {Object} options - The options for `loadTestPage` plus a `runInPage` function.
 */
async function autoTranslatePage(options) {
  const { prefs, ...otherOptions } = options;
  const { cleanup, runInPage } = await loadTestPage({
    autoDownloadFromRemoteSettings: true,
    prefs: [["browser.translations.autoTranslate", true], ...(prefs ?? [])],
    ...otherOptions,
  });
  await runInPage(options.runInPage);
  await cleanup();
}

/**
 * @param {RemoteSettingsClient} client
 * @param {string} mockedCollectionName - The name of the mocked collection without
 *  the incrementing "id" part. This is provided so that attachments can be asserted
 *  as being of a certain version.
 * @param {boolean} autoDownloadFromRemoteSettings - Skip the manual download process,
 *  and automatically download the files. Normally it's preferrable to manually trigger
 *  the downloads to trigger the download behavior, but this flag lets you bypass this
 *  and automatically download the files.
 */
function createAttachmentMock(
  client,
  mockedCollectionName,
  autoDownloadFromRemoteSettings
) {
  const pendingDownloads = [];
  client.attachments.download = record =>
    new Promise((resolve, reject) => {
      console.log("Download requested:", client.collectionName, record.name);
      if (autoDownloadFromRemoteSettings) {
        const encoder = new TextEncoder();
        const { buffer } = encoder.encode(
          `Mocked download: ${mockedCollectionName} ${record.name} ${record.version}`
        );

        resolve({ buffer });
      } else {
        pendingDownloads.push({ record, resolve, reject });
      }
    });

  function resolvePendingDownloads(expectedDownloadCount) {
    info(
      `Resolving ${expectedDownloadCount} mocked downloads for "${client.collectionName}"`
    );
    return downloadHandler(expectedDownloadCount, download =>
      download.resolve({ buffer: new ArrayBuffer() })
    );
  }

  async function rejectPendingDownloads(expectedDownloadCount) {
    info(
      `Intentionally rejecting ${expectedDownloadCount} mocked downloads for "${client.collectionName}"`
    );

    // Add 1 to account for the original attempt.
    const attempts = TranslationsParent.MAX_DOWNLOAD_RETRIES + 1;
    return downloadHandler(expectedDownloadCount * attempts, download =>
      download.reject(new Error("Intentionally rejecting downloads."))
    );
  }

  async function downloadHandler(expectedDownloadCount, action) {
    const names = [];
    let maxTries = 100;
    while (names.length < expectedDownloadCount && maxTries-- > 0) {
      await new Promise(resolve => setTimeout(resolve, 0));
      let download = pendingDownloads.shift();
      if (!download) {
        // Uncomment the following to debug download issues:
        // console.log(`No pending download:`, client.collectionName, names.length);
        continue;
      }
      console.log(`Handling download:`, client.collectionName);
      action(download);
      names.push(download.record.name);
    }

    // This next check is not guaranteed to catch an unexpected download, but wait
    // at least one event loop tick to see if any more downloads were added.
    await new Promise(resolve => setTimeout(resolve, 0));

    if (pendingDownloads.length) {
      throw new Error(
        `An unexpected download was found, only expected ${expectedDownloadCount} downloads`
      );
    }

    return names.sort((a, b) => a.localeCompare(b));
  }

  async function assertNoNewDownloads() {
    await new Promise(resolve => setTimeout(resolve, 0));
    is(
      pendingDownloads.length,
      0,
      `No downloads happened for "${client.collectionName}"`
    );
  }

  return {
    client,
    pendingDownloads,
    resolvePendingDownloads,
    rejectPendingDownloads,
    assertNoNewDownloads,
  };
}

/**
 * The amount of files that are generated per mocked language pair.
 */
const FILES_PER_LANGUAGE_PAIR = 3;

function createRecordsForLanguagePair(fromLang, toLang) {
  const records = [];
  const lang = fromLang + toLang;
  const models = [
    { fileType: "model", name: `model.${lang}.intgemm.alphas.bin` },
    { fileType: "lex", name: `lex.50.50.${lang}.s2t.bin` },
    { fileType: "vocab", name: `vocab.${lang}.spm` },
  ];

  if (models.length !== FILES_PER_LANGUAGE_PAIR) {
    throw new Error("Files per language pair was wrong.");
  }

  for (const { fileType, name } of models) {
    records.push({
      id: crypto.randomUUID(),
      name,
      fromLang,
      toLang,
      fileType,
      version: "1.0",
      last_modified: Date.now(),
      schema: Date.now(),
    });
  }
  return records;
}

/**
 * Increments each time a remote settings client is created to ensure a unique client
 * name for each test run.
 */
let _remoteSettingsMockId = 0;

/**
 * Creates a local RemoteSettingsClient for use within tests.
 *
 * @param {boolean} autoDownloadFromRemoteSettings
 * @param {Object[]} langPairs
 * @returns {RemoteSettingsClient}
 */
async function createTranslationModelsRemoteClient(
  autoDownloadFromRemoteSettings,
  langPairs
) {
  const records = [];
  for (const { fromLang, toLang } of langPairs) {
    records.push(...createRecordsForLanguagePair(fromLang, toLang));
  }

  const { RemoteSettings } = ChromeUtils.importESModule(
    "resource://services-settings/remote-settings.sys.mjs"
  );
  const mockedCollectionName = "test-translation-models";
  const client = RemoteSettings(
    `${mockedCollectionName}-${_remoteSettingsMockId++}`
  );
  const metadata = {};
  await client.db.clear();
  await client.db.importChanges(metadata, Date.now(), records);

  return createAttachmentMock(
    client,
    mockedCollectionName,
    autoDownloadFromRemoteSettings
  );
}

/**
 * Creates a local RemoteSettingsClient for use within tests.
 *
 * @param {boolean} autoDownloadFromRemoteSettings
 * @returns {RemoteSettingsClient}
 */
async function createTranslationsWasmRemoteClient(
  autoDownloadFromRemoteSettings
) {
  const records = ["bergamot-translator", "fasttext-wasm"].map(name => ({
    id: crypto.randomUUID(),
    name,
    version: "1.0",
    last_modified: Date.now(),
    schema: Date.now(),
  }));

  const { RemoteSettings } = ChromeUtils.importESModule(
    "resource://services-settings/remote-settings.sys.mjs"
  );
  const mockedCollectionName = "test-translation-wasm";
  const client = RemoteSettings(
    `${mockedCollectionName}-${_remoteSettingsMockId++}`
  );
  const metadata = {};
  await client.db.clear();
  await client.db.importChanges(metadata, Date.now(), records);

  return createAttachmentMock(
    client,
    mockedCollectionName,
    autoDownloadFromRemoteSettings
  );
}

/**
 * Creates a local RemoteSettingsClient for use within tests.
 *
 * @param {boolean} autoDownloadFromRemoteSettings
 * @returns {RemoteSettingsClient}
 */
async function createLanguageIdModelsRemoteClient(
  autoDownloadFromRemoteSettings
) {
  const records = [
    {
      id: crypto.randomUUID(),
      name: "lid.176.ftz",
      version: "1.0",
      last_modified: Date.now(),
      schema: Date.now(),
    },
  ];

  const { RemoteSettings } = ChromeUtils.importESModule(
    "resource://services-settings/remote-settings.sys.mjs"
  );
  const client = RemoteSettings(
    "test-language-id-models" + _remoteSettingsMockId++
  );
  const mockedCollectionName = "test-language-id-models";
  const metadata = {};
  await client.db.clear();
  await client.db.importChanges(metadata, Date.now(), records);

  return createAttachmentMock(
    client,
    mockedCollectionName,
    autoDownloadFromRemoteSettings
  );
}

async function selectAboutPreferencesElements() {
  const document = gBrowser.selectedBrowser.contentDocument;

  const rows = await waitForCondition(() => {
    const elements = document.querySelectorAll(".translations-manage-language");
    if (elements.length !== 3) {
      return false;
    }
    return elements;
  }, "Waiting for manage language rows.");

  const [downloadAllRow, frenchRow, spanishRow] = rows;

  const downloadAllLabel = downloadAllRow.querySelector("label");
  const downloadAll = downloadAllRow.querySelector(
    "#translations-manage-install-all"
  );
  const deleteAll = downloadAllRow.querySelector(
    "#translations-manage-delete-all"
  );
  const frenchLabel = frenchRow.querySelector("label");
  const frenchDownload = frenchRow.querySelector(
    `[data-l10n-id="translations-manage-language-install-button"]`
  );
  const frenchDelete = frenchRow.querySelector(
    `[data-l10n-id="translations-manage-language-remove-button"]`
  );
  const spanishLabel = spanishRow.querySelector("label");
  const spanishDownload = spanishRow.querySelector(
    `[data-l10n-id="translations-manage-language-install-button"]`
  );
  const spanishDelete = spanishRow.querySelector(
    `[data-l10n-id="translations-manage-language-remove-button"]`
  );

  return {
    document,
    downloadAllLabel,
    downloadAll,
    deleteAll,
    frenchLabel,
    frenchDownload,
    frenchDelete,
    spanishLabel,
    spanishDownload,
    spanishDelete,
  };
}

function click(button, message) {
  info(message);
  if (button.hidden) {
    throw new Error("The button was hidden when trying to click it.");
  }
  button.click();
}

function hitEnterKey(button, message) {
  info(message);
  button.dispatchEvent(
    new KeyboardEvent("keypress", {
      key: "Enter",
      keyCode: KeyboardEvent.DOM_VK_RETURN,
    })
  );
}

/**
 * @param {Object} options
 * @param {string} options.message
 * @param {Record<string, Element[]>} options.visible
 * @param {Record<string, Element[]>} options.hidden
 */
async function assertVisibility({ message, visible, hidden }) {
  info(message);
  try {
    // First wait for the condition to be met.
    await waitForCondition(() => {
      for (const element of Object.values(visible)) {
        if (element.hidden) {
          return false;
        }
      }
      for (const element of Object.values(hidden)) {
        if (!element.hidden) {
          return false;
        }
      }
      return true;
    });
  } catch (error) {
    // Ignore, this will get caught below.
  }
  // Now report the conditions.
  for (const [name, element] of Object.entries(visible)) {
    ok(!element.hidden, `${name} is visible.`);
  }
  for (const [name, element] of Object.entries(hidden)) {
    ok(element.hidden, `${name} is hidden.`);
  }
}

async function setupAboutPreferences(languagePairs) {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Enabled by default.
      ["browser.translations.enable", true],
      ["browser.translations.logLevel", "All"],
    ],
  });
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    BLANK_PAGE,
    true // waitForLoad
  );

  const { remoteClients, removeMocks } = await createAndMockRemoteSettings({
    languagePairs,
  });

  BrowserTestUtils.startLoadingURIString(
    tab.linkedBrowser,
    "about:preferences"
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  const elements = await selectAboutPreferencesElements();

  async function cleanup() {
    await closeTranslationsPanelIfOpen();
    gBrowser.removeCurrentTab();
    await removeMocks();
    await SpecialPowers.popPrefEnv();
    TestTranslationsTelemetry.reset();
  }

  return {
    cleanup,
    remoteClients,
    elements,
  };
}

function waitForAppLocaleChanged() {
  new Promise(resolve => {
    function onChange() {
      Services.obs.removeObserver(onChange, "intl:app-locales-changed");
      resolve();
    }
    Services.obs.addObserver(onChange, "intl:app-locales-changed");
  });
}

async function mockLocales({ systemLocales, appLocales, webLanguages }) {
  const appLocaleChanged1 = waitForAppLocaleChanged();

  TranslationsParent.mockedSystemLocales = systemLocales;
  const { availableLocales, requestedLocales } = Services.locale;

  info("Mocking locales, so expect potential .ftl resource errors.");
  Services.locale.availableLocales = appLocales;
  Services.locale.requestedLocales = appLocales;

  await appLocaleChanged1;

  await SpecialPowers.pushPrefEnv({
    set: [["intl.accept_languages", webLanguages.join(",")]],
  });

  return async () => {
    const appLocaleChanged2 = waitForAppLocaleChanged();

    // Reset back to the originals.
    TranslationsParent.mockedSystemLocales = null;
    Services.locale.availableLocales = availableLocales;
    Services.locale.requestedLocales = requestedLocales;

    await appLocaleChanged2;

    await SpecialPowers.popPrefEnv();
  };
}

/**
 * Helpful test functions for translations telemetry
 */
class TestTranslationsTelemetry {
  static #previousFlowId = null;

  static reset() {
    TestTranslationsTelemetry.#previousFlowId = null;
  }

  /**
   * Asserts qualities about a counter telemetry metric.
   *
   * @param {string} name - The name of the metric.
   * @param {Object} counter - The Glean counter object.
   * @param {Object} expectedCount - The expected value of the counter.
   */
  static async assertCounter(name, counter, expectedCount) {
    // Ensures that glean metrics are collected from all child processes
    // so that calls to testGetValue() are up to date.
    await Services.fog.testFlushAllChildren();
    const count = counter.testGetValue() ?? 0;
    is(
      count,
      expectedCount,
      `Telemetry counter ${name} should have expected count`
    );
  }

  /**
   * Asserts qualities about an event telemetry metric.
   *
   * @param {string} name - The name of the metric.
   * @param {Object} event - The Glean event object.
   * @param {Object} expectations - The test expectations.
   * @param {number} expectations.expectedEventCount - The expected count of events.
   * @param {boolean} expectations.expectNewFlowId
   * - Expects the flowId to be different than the previous flowId if true,
   *   and expects it to be the same if false.
   * @param {Array<function>} [expectations.allValuePredicates=[]]
   * - An array of function predicates to assert for all event values.
   * @param {Array<function>} [expectations.finalValuePredicates=[]]
   * - An array of function predicates to assert for only the final event value.
   */
  static async assertEvent(
    event,
    {
      expectedEventCount,
      expectNewFlowId = null,
      expectFirstInteraction = null,
      allValuePredicates = [],
      finalValuePredicates = [],
    }
  ) {
    // Ensures that glean metrics are collected from all child processes
    // so that calls to testGetValue() are up to date.
    await Services.fog.testFlushAllChildren();
    const events = event.testGetValue() ?? [];
    const eventCount = events.length;
    const name =
      eventCount > 0 ? `${events[0].category}.${events[0].name}` : null;

    if (eventCount > 0 && expectFirstInteraction !== null) {
      is(
        events[eventCount - 1].extra.first_interaction,
        expectFirstInteraction ? "true" : "false",
        "The newest event should be match the given first-interaction expectation"
      );
    }

    if (eventCount > 0 && expectNewFlowId !== null) {
      const flowId = events[eventCount - 1].extra.flow_id;
      if (expectNewFlowId) {
        is(
          events[eventCount - 1].extra.flow_id !==
            TestTranslationsTelemetry.#previousFlowId,
          true,
          `The newest flowId ${flowId} should be different than the previous flowId ${
            TestTranslationsTelemetry.#previousFlowId
          }`
        );
      } else {
        is(
          events[eventCount - 1].extra.flow_id ===
            TestTranslationsTelemetry.#previousFlowId,
          true,
          `The newest flowId ${flowId} should be equal to the previous flowId ${
            TestTranslationsTelemetry.#previousFlowId
          }`
        );
      }
      TestTranslationsTelemetry.#previousFlowId = flowId;
    }

    is(
      eventCount,
      expectedEventCount,
      `There should be ${expectedEventCount} telemetry events of type ${name}`
    );

    if (allValuePredicates.length !== 0) {
      is(
        eventCount > 0,
        true,
        `Telemetry event ${name} should contain values if allPredicates are specified`
      );
      for (const value of events) {
        for (const predicate of allValuePredicates) {
          is(
            predicate(value),
            true,
            `Telemetry event ${name} allPredicate { ${predicate.toString()} } should pass for each value`
          );
        }
      }
    }

    if (finalValuePredicates.length !== 0) {
      is(
        eventCount > 0,
        true,
        `Telemetry event ${name} should contain values if finalPredicates are specified`
      );
      for (const predicate of finalValuePredicates) {
        is(
          predicate(events[eventCount - 1]),
          true,
          `Telemetry event ${name} finalPredicate { ${predicate.toString()} } should pass for final value`
        );
      }
    }
  }

  /**
   * Asserts qualities about a rate telemetry metric.
   *
   * @param {string} name - The name of the metric.
   * @param {Object} rate - The Glean rate object.
   * @param {Object} expectations - The test expectations.
   * @param {number} expectations.expectedNumerator - The expected value of the numerator.
   * @param {number} expectations.expectedDenominator - The expected value of the denominator.
   */
  static async assertRate(
    name,
    rate,
    { expectedNumerator, expectedDenominator }
  ) {
    // Ensures that glean metrics are collected from all child processes
    // so that calls to testGetValue() are up to date.
    await Services.fog.testFlushAllChildren();
    const { numerator = 0, denominator = 0 } = rate.testGetValue() ?? {};
    is(
      numerator,
      expectedNumerator,
      `Telemetry rate ${name} should have expected numerator`
    );
    is(
      denominator,
      expectedDenominator,
      `Telemetry rate ${name} should have expected denominator`
    );
  }
}

/**
 * Provide longer defaults for the waitForCondition.
 *
 * @param {Function} callback
 * @param {string} messages
 */
function waitForCondition(callback, message) {
  const interval = 100;
  // Use 4 times the defaults to guard against intermittents. Many of the tests rely on
  // communication between the parent and child process, which is inherently async.
  const maxTries = 50 * 4;
  return TestUtils.waitForCondition(callback, message, interval, maxTries);
}
