/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TranslationsParent } = ChromeUtils.importESModule(
  "resource://gre/actors/TranslationsParent.sys.mjs"
);

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
 * @param {Array<{ fromLang: string, toLang: string}>} options.languagePairs
 * The translation languages pairs to mock for the test.
 *
 * @param {Array<[string, string]>} options.prefs
 * Prefs to push on for the test.
 */
async function openAboutTranslations({
  dataForContent,
  disabled,
  runInPage,
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
  };

  // Start the tab at about:blank.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank",
    true // waitForLoad
  );

  // Before loading about:translations, handle any mocking of the actor.
  if (languagePairs) {
    TranslationsParent.mock(languagePairs);
  }

  // Now load the about:translations page, since the actor could be mocked.
  BrowserTestUtils.loadURIString(tab.linkedBrowser, "about:translations");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  await ContentTask.spawn(
    tab.linkedBrowser,
    { dataForContent, selectors },
    runInPage
  );

  if (languagePairs) {
    TranslationsParent.mock(null);
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
