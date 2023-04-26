/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Avoid about:blank's non-standard behavior.
const BLANK_PAGE =
  "data:text/html;charset=utf-8,<!DOCTYPE html><title>Blank</title>Blank page";

const URL_PREFIX = "https://example.com/browser/";
const CHROME_URL_PREFIX = "chrome://mochitests/content/browser/";
const DIR_PATH = "toolkit/components/translations/tests/browser/";
const TRANSLATIONS_TESTER_EN =
  URL_PREFIX + DIR_PATH + "translations-tester-en.html";
const TRANSLATIONS_TESTER_ES =
  URL_PREFIX + DIR_PATH + "translations-tester-es.html";
const TRANSLATIONS_TESTER_NO_TAG =
  URL_PREFIX + DIR_PATH + "translations-tester-no-tag.html";

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
 * @param {string} detectedLanguageLabel
 * This is the two-letter language label for the MockedLanguageIdEngine to return as
 * the mocked detected language.
 *
 * @param {Array<{ fromLang: string, toLang: string, isBeta: boolean }>} options.languagePairs
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
  detectedLanguageLabel,
  languagePairs,
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

  if (languagePairs) {
    // Before loading about:translations, handle the mocking of the actor.
    TranslationsParent.mockLanguagePairs(languagePairs);
  }
  TranslationsParent.mockLanguageIdentification(
    detectedLanguageLabel ?? "en",
    detectedLanguageConfidence ?? "0.5"
  );

  // Now load the about:translations page, since the actor could be mocked.
  BrowserTestUtils.loadURIString(tab.linkedBrowser, "about:translations");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  await ContentTask.spawn(
    tab.linkedBrowser,
    { dataForContent, selectors },
    runInPage
  );

  if (languagePairs) {
    TranslationsParent.mockLanguagePairs(null);
  }
  if (detectedLanguageLabel && detectedLanguageConfidence) {
    TranslationsParent.mockLanguageIdentification(null, null);
  }
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
 * This is for tests that don't need a browser page to run.
 */
async function setupActorTest({
  languagePairs,
  prefs,
  detectedLanguageConfidence,
  detectedLanguageLabel,
}) {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Enabled by default.
      ["browser.translations.enable", true],
      ["browser.translations.logLevel", "All"],
      ...(prefs ?? []),
    ],
  });

  if (languagePairs) {
    const translationModels = await createTranslationModelsRemoteClient(
      languagePairs
    );
    TranslationsParent.translationModelsRemoteClient = translationModels.client;
  }

  if (detectedLanguageLabel && detectedLanguageConfidence) {
    TranslationsParent.mockLanguageIdentification(
      detectedLanguageLabel,
      detectedLanguageConfidence
    );
  }

  /** @type {import("../../actors/TranslationsParent.sys.mjs").TranslationsParent} */
  const actor = gBrowser.selectedBrowser.browsingContext.currentWindowGlobal.getActor(
    "Translations"
  );

  return {
    actor,
    cleanup() {
      TranslationsParent.translationModelsRemoteClient = null;
      TranslationsParent.mockLanguageIdentification(null, null);
      return SpecialPowers.popPrefEnv();
    },
  };
}

async function loadTestPage({
  languagePairs,
  detectedLanguageConfidence,
  detectedLanguageLabel,
  page,
  prefs,
}) {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Enabled by default.
      ["browser.translations.enable", true],
      ["browser.translations.logLevel", "All"],
      ...(prefs ?? []),
    ],
  });

  // Start the tab at a blank page.
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    BLANK_PAGE,
    true // waitForLoad
  );

  // Before loading the page, handle any mocking of the actor.
  if (languagePairs) {
    TranslationsParent.mockLanguagePairs(languagePairs);
  }

  if (detectedLanguageLabel && detectedLanguageConfidence) {
    TranslationsParent.mockLanguageIdentification(
      detectedLanguageLabel,
      detectedLanguageConfidence
    );
  }

  BrowserTestUtils.loadURIString(tab.linkedBrowser, page);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  return {
    tab,

    /**
     * @returns {Promise<void>}
     */
    cleanup() {
      if (languagePairs) {
        TranslationsParent.mockLanguagePairs(null);
      }

      if (detectedLanguageLabel && detectedLanguageConfidence) {
        TranslationsParent.mockLanguageIdentification(null, null);
      }

      BrowserTestUtils.removeTab(tab);
      return SpecialPowers.popPrefEnv();
    },

    /**
     * Runs a callback in the content page. The function's contents are serialized as
     * a string, and run in the page. The `translations-test.mjs` module is made
     * available to the page.
     *
     * @param {(TranslationsTest: import("./translations-test.mjs")) => any} callback
     * @returns {Promise<void>}
     */
    runInPage(callback) {
      // ContentTask.spawn runs the `Function.prototype.toString` on this function in
      // order to send it into the content process. The following function is doing its
      // own string manipulation in order to load in the TranslationsTest module.
      const fn = new Function(/* js */ `
        const TranslationsTest = ChromeUtils.importESModule(
          "chrome://mochitests/content/browser/toolkit/components/translations/tests/browser/translations-test.mjs"
        );

        // Pass in the values that get injected by the task runner.
        TranslationsTest.setup({Assert, ContentTaskUtils, content});

        return (${callback.toString()})(TranslationsTest);
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
 * @param {Object} options - The options for `loadTestPage` plus a `runInPage` function.
 */
async function loadTestPageAndRun(options) {
  const { cleanup, runInPage } = await loadTestPage(options);
  await runInPage(options.runInPage);
  await cleanup();
}

/**
 * @param {RemoteSettingsClient} client
 */
function createAttachmentMock(client) {
  const pendingDownloads = [];
  client.attachments.download = record =>
    new Promise((resolve, reject) => {
      pendingDownloads.push({ record, resolve, reject });
    });

  function resolvePendingDownloads(expectedDownloadCount) {
    info(`Resolving mocked downloads for "${client.collectionName}"`);
    return downloadHandler(expectedDownloadCount, download =>
      download.resolve({ buffer: new ArrayBuffer() })
    );
  }

  async function rejectPendingDownloads(expectedDownloadCount) {
    info(
      `Intentionally rejecting mocked downloads for "${client.collectionName}"`
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
        continue;
      }
      action(download);
      names.push(download.record.name);
    }

    // This next check is not guaranteed to catch an unexpected download, but go ahead
    // and wait two event loop ticks in order to catch any stray downloads.
    await new Promise(resolve => setTimeout(resolve, 0));
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
 * Creates a local RemoteSettingsClient for use within tests.
 *
 * @param {Object[]} langPairs
 * @returns {RemoteSettingsClient}
 */
async function createTranslationModelsRemoteClient(langPairs) {
  const records = [];
  for (const { fromLang, toLang } of langPairs) {
    const lang = fromLang + toLang;
    const models = [
      { fileType: "model", name: `model.${lang}.intgemm.alphas.bin` },
      { fileType: "lex", name: `lex.50.50.${lang}.s2t.bin` },
      { fileType: "qualityModel", name: `qualityModel.${lang}.bin` },
      { fileType: "vocab", name: `vocab.${lang}.spm` },
    ];

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
  }

  const { RemoteSettings } = ChromeUtils.import(
    "resource://services-settings/remote-settings.js"
  );
  const client = RemoteSettings("test-translation-models");
  const metadata = {};
  await client.db.clear();
  await client.db.importChanges(metadata, Date.now(), records);

  return createAttachmentMock(client);
}

/**
 * Creates a local RemoteSettingsClient for use within tests.
 *
 * @returns {RemoteSettingsClient}
 */
async function createTranslationsWasmRemoteClient() {
  const records = ["bergamot-translator", "fasttext-wasm"].map(name => ({
    id: crypto.randomUUID(),
    name,
    version: "1.0",
    last_modified: Date.now(),
    schema: Date.now(),
  }));

  const { RemoteSettings } = ChromeUtils.import(
    "resource://services-settings/remote-settings.js"
  );
  const client = RemoteSettings("test-translations-wasm");
  const metadata = {};
  await client.db.clear();
  await client.db.importChanges(metadata, Date.now(), records);

  return createAttachmentMock(client);
}

/**
 * Creates a local RemoteSettingsClient for use within tests.
 *
 * @returns {RemoteSettingsClient}
 */
async function createLanguageIdModelsRemoteClient() {
  const records = [
    {
      id: crypto.randomUUID(),
      name: "lid.176.ftz",
      version: "1.0",
      last_modified: Date.now(),
      schema: Date.now(),
    },
  ];

  const { RemoteSettings } = ChromeUtils.import(
    "resource://services-settings/remote-settings.js"
  );
  const client = RemoteSettings("test-language-id-models");
  const metadata = {};
  await client.db.clear();
  await client.db.importChanges(metadata, Date.now(), records);

  return createAttachmentMock(client);
}

async function selectAboutPreferencesElements() {
  const document = gBrowser.selectedBrowser.contentDocument;

  const rows = await TestUtils.waitForCondition(() => {
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
    `[data-l10n-id="translations-manage-download-button"]`
  );
  const frenchDelete = frenchRow.querySelector(
    `[data-l10n-id="translations-manage-delete-button"]`
  );
  const spanishLabel = spanishRow.querySelector("label");
  const spanishDownload = spanishRow.querySelector(
    `[data-l10n-id="translations-manage-download-button"]`
  );
  const spanishDelete = spanishRow.querySelector(
    `[data-l10n-id="translations-manage-delete-button"]`
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
    await TestUtils.waitForCondition(() => {
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

  const remoteClients = {
    translationModels: await createTranslationModelsRemoteClient(languagePairs),
    translationsWasm: await createTranslationsWasmRemoteClient(),
    languageIdModels: await createLanguageIdModelsRemoteClient(),
  };

  TranslationsParent.translationModelsRemoteClient =
    remoteClients.translationModels.client;
  TranslationsParent.translationsWasmRemoteClient =
    remoteClients.translationsWasm.client;
  TranslationsParent.languageIdModelsRemoteClient =
    remoteClients.languageIdModels.client;

  BrowserTestUtils.loadURIString(tab.linkedBrowser, "about:preferences");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  const elements = await selectAboutPreferencesElements();

  async function cleanup() {
    gBrowser.removeCurrentTab();
    TranslationsParent.translationModelsRemoteClient = null;
    TranslationsParent.translationsWasmRemoteClient = null;
    TranslationsParent.languageIdModelsRemoteClient = null;

    await SpecialPowers.popPrefEnv();
  }

  return {
    cleanup,
    remoteClients,
    elements,
  };
}
