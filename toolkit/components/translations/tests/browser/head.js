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
