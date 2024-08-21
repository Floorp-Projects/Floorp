/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This file contains unit tests for the translations actor. Generally it's preferable
 * to test behavior in a full integration test, but occasionally it's useful to test
 * specific implementation behavior.
 */

/**
 * Enforce the pivot language behavior in ensureLanguagePairsHavePivots.
 */
add_task(async function test_pivot_language_behavior() {
  info(
    "Expect 4 console.error messages notifying of the lack of a pivot language."
  );

  const fromLanguagePairs = [
    { fromLang: "en", toLang: "es" },
    { fromLang: "es", toLang: "en" },
    { fromLang: "en", toLang: "yue" },
    { fromLang: "yue", toLang: "en" },
    // This is not a bi-directional translation.
    { fromLang: "is", toLang: "en" },
    // These are non-pivot languages.
    { fromLang: "zh", toLang: "ja" },
    { fromLang: "ja", toLang: "zh" },
  ];

  // Sort the language pairs, as the order is not guaranteed.
  function sort(list) {
    return list.sort((a, b) =>
      `${a.fromLang}-${a.toLang}`.localeCompare(`${b.fromLang}-${b.toLang}`)
    );
  }

  const { cleanup } = await setupActorTest({
    languagePairs: fromLanguagePairs,
  });

  const { languagePairs } = await TranslationsParent.getSupportedLanguages();

  // The pairs aren't guaranteed to be sorted.
  languagePairs.sort((a, b) =>
    TranslationsParent.languagePairKey(a.fromLang, a.toLang).localeCompare(
      TranslationsParent.languagePairKey(b.fromLang, b.toLang)
    )
  );

  if (SpecialPowers.isDebugBuild) {
    Assert.deepEqual(
      sort(languagePairs),
      sort([
        { fromLang: "en", toLang: "es" },
        { fromLang: "en", toLang: "yue" },
        { fromLang: "es", toLang: "en" },
        { fromLang: "is", toLang: "en" },
        { fromLang: "yue", toLang: "en" },
      ]),
      "Non-pivot languages were removed on debug builds."
    );
  } else {
    Assert.deepEqual(
      sort(languagePairs),
      sort(fromLanguagePairs),
      "Non-pivot languages are retained on non-debug builds."
    );
  }

  await cleanup();
});

/**
 * This test case ensures that stand-alone functions to check language support match the
 * state of the available language pairs.
 */
add_task(async function test_language_support_checks() {
  const { cleanup } = await setupActorTest({
    languagePairs: [
      { fromLang: PIVOT_LANGUAGE, toLang: "es" },
      { fromLang: "es", toLang: PIVOT_LANGUAGE },
      { fromLang: PIVOT_LANGUAGE, toLang: "fr" },
      { fromLang: "fr", toLang: PIVOT_LANGUAGE },
      { fromLang: PIVOT_LANGUAGE, toLang: "pl" },
      { fromLang: "pl", toLang: PIVOT_LANGUAGE },
      // Only supported as a source language
      { fromLang: "fi", toLang: PIVOT_LANGUAGE },
      // Only supported as a target language
      { fromLang: PIVOT_LANGUAGE, toLang: "sl" },
    ],
  });

  const { languagePairs } = await TranslationsParent.getSupportedLanguages();
  for (const { fromLang, toLang } of languagePairs) {
    ok(
      await TranslationsParent.isSupportedAsFromLang(fromLang),
      "Each from-language should be supported as a translation source language."
    );

    ok(
      await TranslationsParent.isSupportedAsToLang(toLang),
      "Each to-language should be supported as a translation target language."
    );

    is(
      await TranslationsParent.isSupportedAsToLang(fromLang),
      languagePairs.some(({ toLang }) => toLang === fromLang),
      "A from-language should be supported as a to-language if it also exists in the to-language list."
    );

    is(
      await TranslationsParent.isSupportedAsFromLang(toLang),
      languagePairs.some(({ fromLang }) => fromLang === toLang),
      "A to-language should be supported as a from-language if it also exists in the from-language list."
    );
  }

  await usingAppLocale("en", async () => {
    const expected = "en";
    const actual = await TranslationsParent.getTopPreferredSupportedToLang();
    is(
      actual,
      expected,
      "The top supported to-language should match the expected language tag"
    );
  });

  await usingAppLocale("es", async () => {
    const expected = "es";
    const actual = await TranslationsParent.getTopPreferredSupportedToLang();
    is(
      actual,
      expected,
      "The top supported to-language should match the expected language tag"
    );
  });

  // Only supported as a source language
  await usingAppLocale("fi", async () => {
    const expected = "en";
    const actual = await TranslationsParent.getTopPreferredSupportedToLang();
    is(
      actual,
      expected,
      "The top supported to-language should match the expected language tag"
    );
  });

  // Only supported as a target language
  await usingAppLocale("sl", async () => {
    const expected = "sl";
    const actual = await TranslationsParent.getTopPreferredSupportedToLang();
    is(
      actual,
      expected,
      "The top supported to-language should match the expected language tag"
    );
  });

  // Not supported as a source language or a target language.
  await usingAppLocale("ja", async () => {
    const expected = "en";
    const actual = await TranslationsParent.getTopPreferredSupportedToLang();
    is(
      actual,
      expected,
      "The top supported to-language should match the expected language tag"
    );
  });

  await cleanup();
});

async function usingAppLocale(locale, callback) {
  info(`Mocking the locale "${locale}", expect missing resource errors.`);
  const cleanupLocales = await mockLocales({
    appLocales: [locale],
  });

  if (Services.locale.appLocaleAsBCP47 !== locale) {
    throw new Error("Unable to change the app locale.");
  }
  await callback();

  // Reset back to the originals.
  await cleanupLocales();
}

add_task(async function test_translating_to_and_from_app_language() {
  const PIVOT_LANGUAGE = "en";

  const { cleanup } = await setupActorTest({
    languagePairs: [
      { fromLang: PIVOT_LANGUAGE, toLang: "es" },
      { fromLang: "es", toLang: PIVOT_LANGUAGE },
      { fromLang: PIVOT_LANGUAGE, toLang: "fr" },
      { fromLang: "fr", toLang: PIVOT_LANGUAGE },
      { fromLang: PIVOT_LANGUAGE, toLang: "pl" },
      { fromLang: "pl", toLang: PIVOT_LANGUAGE },
    ],
  });

  /**
   * Each language pair has multiple models. De-duplicate the language pairs and
   * return a sorted list.
   */
  function getUniqueLanguagePairs(records) {
    const langPairs = new Set();
    for (const { fromLang, toLang } of records) {
      langPairs.add(TranslationsParent.languagePairKey(fromLang, toLang));
    }
    return Array.from(langPairs)
      .sort()
      .map(langPair => {
        const [fromLang, toLang] = langPair.split(",");
        return {
          fromLang,
          toLang,
        };
      });
  }

  function assertLanguagePairs({
    app,
    requested,
    message,
    languagePairs,
    isForDeletion,
  }) {
    return usingAppLocale(app, async () => {
      Assert.deepEqual(
        getUniqueLanguagePairs(
          await TranslationsParent.getRecordsForTranslatingToAndFromAppLanguage(
            requested,
            isForDeletion
          )
        ),
        languagePairs,
        message
      );
    });
  }

  await assertLanguagePairs({
    message:
      "When the app locale is the pivot language, download another language.",
    app: PIVOT_LANGUAGE,
    requested: "fr",
    languagePairs: [
      { fromLang: PIVOT_LANGUAGE, toLang: "fr" },
      { fromLang: "fr", toLang: PIVOT_LANGUAGE },
    ],
  });

  await assertLanguagePairs({
    message: "When a pivot language is required, they are both downloaded.",
    app: "fr",
    requested: "pl",
    languagePairs: [
      { fromLang: PIVOT_LANGUAGE, toLang: "fr" },
      { fromLang: PIVOT_LANGUAGE, toLang: "pl" },
      { fromLang: "fr", toLang: PIVOT_LANGUAGE },
      { fromLang: "pl", toLang: PIVOT_LANGUAGE },
    ],
  });

  await assertLanguagePairs({
    message:
      "When downloading the pivot language, only download the one for the app's locale.",
    app: "es",
    requested: PIVOT_LANGUAGE,
    languagePairs: [
      { fromLang: PIVOT_LANGUAGE, toLang: "es" },
      { fromLang: "es", toLang: PIVOT_LANGUAGE },
    ],
  });

  await assertLanguagePairs({
    message:
      "Delete just the requested language when the app locale is the pivot language",
    app: PIVOT_LANGUAGE,
    requested: "fr",
    isForDeletion: true,
    languagePairs: [
      { fromLang: PIVOT_LANGUAGE, toLang: "fr" },
      { fromLang: "fr", toLang: PIVOT_LANGUAGE },
    ],
  });

  await assertLanguagePairs({
    message: "Delete just the requested language, and not the pivot.",
    app: "fr",
    requested: "pl",
    isForDeletion: true,
    languagePairs: [
      { fromLang: PIVOT_LANGUAGE, toLang: "pl" },
      { fromLang: "pl", toLang: PIVOT_LANGUAGE },
    ],
  });

  await assertLanguagePairs({
    message: "Delete just the requested language, and not the pivot.",
    app: "fr",
    requested: "pl",
    isForDeletion: true,
    languagePairs: [
      { fromLang: PIVOT_LANGUAGE, toLang: "pl" },
      { fromLang: "pl", toLang: PIVOT_LANGUAGE },
    ],
  });

  await assertLanguagePairs({
    message: "Delete just the pivot → app and app → pivot.",
    app: "es",
    requested: PIVOT_LANGUAGE,
    isForDeletion: true,
    languagePairs: [
      { fromLang: PIVOT_LANGUAGE, toLang: "es" },
      { fromLang: "es", toLang: PIVOT_LANGUAGE },
    ],
  });

  await assertLanguagePairs({
    message:
      "If the app and request language are the same, nothing is returned.",
    app: "fr",
    requested: "fr",
    languagePairs: [],
  });

  await cleanup();
});

add_task(async function test_firstVisualChange() {
  const { cleanup } = await setupActorTest({
    languagePairs: [{ fromLang: "en", toLang: "es" }],
  });

  const parent = getTranslationsParent();

  Assert.equal(
    parent.languageState.hasVisibleChange,
    false,
    "No visual translation change has occurred yet"
  );

  parent.receiveMessage({
    name: "Translations:ReportFirstVisibleChange",
  });

  Assert.equal(
    parent.languageState.hasVisibleChange,
    true,
    "A change occurred."
  );

  await cleanup();
});
