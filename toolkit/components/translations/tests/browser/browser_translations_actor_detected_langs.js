/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_detected_language() {
  const { cleanup, tab } = await loadTestPage({
    // This page will get its language changed by the test.
    page: ENGLISH_PAGE_URL,
    autoDownloadFromRemoteSettings: true,
    languagePairs: [
      // Spanish
      { fromLang: "es", toLang: "en" },
      { fromLang: "en", toLang: "es" },

      // Norwegian Bokmål
      { fromLang: "nb", toLang: "en" },
      { fromLang: "en", toLang: "nb" },
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

  Assert.deepEqual(
    await getDetectedLanguagesFor("es"),
    {
      docLangTag: "es",
      userLangTag: "en",
      isDocLangTagSupported: true,
    },
    "Spanish is detected as a supported language."
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("chr"),
    {
      docLangTag: "chr",
      userLangTag: "en",
      isDocLangTagSupported: false,
    },
    "Cherokee is detected, but is not a supported language."
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("no"),
    {
      docLangTag: "nb",
      userLangTag: "en",
      isDocLangTagSupported: true,
    },
    "The Norwegian macro language is detected, but it defaults to Norwegian Bokmål."
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("spa"),
    {
      docLangTag: "es",
      userLangTag: "en",
      isDocLangTagSupported: true,
    },
    'The three letter "spa" locale is canonicalized to "es".'
  );

  Assert.deepEqual(
    await getDetectedLanguagesFor("gibberish"),
    {
      docLangTag: "en",
      userLangTag: null,
      isDocLangTagSupported: true,
    },
    "A gibberish locale is discarded, and the language is detected."
  );

  return cleanup();
});
