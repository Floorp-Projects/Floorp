/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This file contains unit tests for the translations actor. Generally it's preferrable
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

  const { actor, cleanup } = await setupActorTest({
    languagePairs: [
      { fromLang: "en", toLang: "es" },
      { fromLang: "es", toLang: "en" },
      // This is not a bi-directional translation.
      { fromLang: "is", toLang: "en" },
      // These are non-pivot languages.
      { fromLang: "zh", toLang: "ja" },
      { fromLang: "ja", toLang: "zh" },
    ],
  });

  const { languagePairs } = await actor.getSupportedLanguages();

  Assert.deepEqual(
    languagePairs,
    [
      { fromLang: "en", toLang: "es" },
      { fromLang: "es", toLang: "en" },
      { fromLang: "is", toLang: "en" },
    ],
    "Non-pivot languages were removed."
  );

  return cleanup();
});
