/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test some corner cases from Bug 1849815 where empty web languages were causing
 * issues.
 */
add_task(async function test_detected_language() {
  const { cleanup, tab } = await loadTestPage({
    // This page will get its language changed by the test.
    page: ENGLISH_PAGE_URL,
    autoDownloadFromRemoteSettings: true,
    // Empty out the accept languages.
    languagePairs: [
      // Spanish
      { fromLang: "es", toLang: "en" },
      { fromLang: "en", toLang: "es" },
      // French
      { fromLang: "fr", toLang: "en" },
      { fromLang: "en", toLang: "fr" },
    ],
  });

  async function getDetectedLanguagesFor(docLangTag) {
    await ContentTask.spawn(
      tab.linkedBrowser,
      { docLangTag },
      function changeLanguage({ docLangTag }) {
        content.document.body.parentNode.setAttribute("lang", docLangTag);
      }
    );
    // Clear out the cached values.
    getTranslationsParent().languageState.detectedLanguages = null;
    return getTranslationsParent().getDetectedLanguages(docLangTag);
  }

  {
    const cleanupLocales = await mockLocales({
      systemLocales: ["en"],
      appLocales: ["en"],
      webLanguages: [""],
    });

    Assert.deepEqual(
      await getDetectedLanguagesFor("en"),
      {
        docLangTag: "en",
        userLangTag: null,
        isDocLangTagSupported: true,
      },
      "If the web languages are empty, do not offer a language matching the app locale."
    );

    await cleanupLocales();
  }

  {
    const cleanupLocales = await mockLocales({
      systemLocales: ["en", "es"],
      appLocales: ["en"],
      webLanguages: [""],
    });

    Assert.deepEqual(
      await getDetectedLanguagesFor("en"),
      {
        docLangTag: "en",
        userLangTag: null,
        isDocLangTagSupported: true,
      },
      "When there are multiple system locales, the app locale is used."
    );

    Assert.deepEqual(
      await getDetectedLanguagesFor("es"),
      {
        docLangTag: "es",
        userLangTag: "en",
        isDocLangTagSupported: true,
      },
      "When there are multiple system locales, the app locale is used."
    );

    await cleanupLocales();
  }

  return cleanup();
});
