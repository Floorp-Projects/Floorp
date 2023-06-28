/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PIVOT_LANGUAGE = "en";

const LANGUAGE_PAIRS = [
  { fromLang: PIVOT_LANGUAGE, toLang: "es" },
  { fromLang: "es", toLang: PIVOT_LANGUAGE },
  { fromLang: PIVOT_LANGUAGE, toLang: "fr" },
  { fromLang: "fr", toLang: PIVOT_LANGUAGE },
  { fromLang: PIVOT_LANGUAGE, toLang: "pl" },
  { fromLang: "pl", toLang: PIVOT_LANGUAGE },
];

async function runLangTagsTest(
  {
    systemLocales,
    appLocales,
    webLanguages,
    page,
    languagePairs = LANGUAGE_PAIRS,
  },
  langTags
) {
  const cleanupLocales = await mockLocales({
    systemLocales,
    appLocales,
    webLanguages,
  });

  const { cleanup: cleanupTestPage } = await loadTestPage({
    page,
    languagePairs,
  });
  const actor = getTranslationsParent();

  await waitForCondition(
    async () => actor.languageState.detectedLanguages?.docLangTag,
    "Waiting for a document language tag to be found."
  );

  Assert.deepEqual(actor.languageState.detectedLanguages, langTags);

  await cleanupLocales();
  await cleanupTestPage();
}

add_task(async function test_lang_tags_direct_translations() {
  info(
    "Test the detected languages for translations when a translation pair is available"
  );
  await runLangTagsTest(
    {
      systemLocales: ["en"],
      appLocales: ["en"],
      webLanguages: ["en"],
      page: TRANSLATIONS_TESTER_ES,
    },
    {
      docLangTag: "es",
      userLangTag: "en",
      isDocLangTagSupported: true,
    }
  );
});

add_task(async function test_lang_tags_with_pivots() {
  info("Test the detected languages for translations when a pivot is needed.");
  await runLangTagsTest(
    {
      systemLocales: ["fr"],
      appLocales: ["fr", "en"],
      webLanguages: ["fr", "en"],
      page: TRANSLATIONS_TESTER_ES,
    },
    {
      docLangTag: "es",
      userLangTag: "fr",
      isDocLangTagSupported: true,
    }
  );
});

add_task(async function test_lang_tags_with_pivots_second_preferred() {
  info(
    "Test using a pivot language when the first preferred lang tag doesn't match"
  );
  await runLangTagsTest(
    {
      systemLocales: ["it"],
      appLocales: ["it", "en"],
      webLanguages: ["it", "en"],
      page: TRANSLATIONS_TESTER_ES,
    },
    {
      docLangTag: "es",
      userLangTag: "en",
      isDocLangTagSupported: true,
    }
  );
});

add_task(async function test_lang_tags_with_non_supported_doc_language() {
  info("Test using a pivot language when the doc language isn't supported");
  await runLangTagsTest(
    {
      systemLocales: ["fr"],
      appLocales: ["fr", "en"],
      webLanguages: ["fr", "en"],
      page: TRANSLATIONS_TESTER_ES,
      languagePairs: [
        { fromLang: PIVOT_LANGUAGE, toLang: "fr" },
        { fromLang: "fr", toLang: PIVOT_LANGUAGE },
        // No Spanish support.
      ],
    },
    {
      docLangTag: "es",
      userLangTag: "fr",
      isDocLangTagSupported: false,
    }
  );
});
