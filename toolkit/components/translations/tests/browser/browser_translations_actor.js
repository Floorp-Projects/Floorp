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

  return cleanup();
});

async function usingAppLocale(locale, callback) {
  info(`Mocking the locale "${locale}", expect missing resource errors.`);
  const { availableLocales, requestedLocales } = Services.locale;
  Services.locale.availableLocales = [locale];
  Services.locale.requestedLocales = [locale];

  if (Services.locale.appLocaleAsBCP47 !== locale) {
    throw new Error("Unable to change the app locale.");
  }
  await callback();

  // Reset back to the originals.
  Services.locale.availableLocales = availableLocales;
  Services.locale.requestedLocales = requestedLocales;
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

  return cleanup();
});
