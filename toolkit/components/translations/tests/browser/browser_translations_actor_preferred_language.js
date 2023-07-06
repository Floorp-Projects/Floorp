/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function waitForAppLocaleChanged() {
  new Promise(resolve => {
    function onChange() {
      Services.obs.removeObserver(onChange, "intl:app-locales-changed");
      resolve();
    }
    Services.obs.addObserver(onChange, "intl:app-locales-changed");
  });
}

async function testWithLocales({
  systemLocales,
  appLocales,
  webLanguages,
  test,
}) {
  const cleanup = await mockLocales({
    systemLocales,
    appLocales,
    webLanguages,
  });
  test();
  return cleanup();
}

add_task(async function test_preferred_language() {
  await testWithLocales({
    systemLocales: ["en-US"],
    appLocales: ["en-US"],
    webLanguages: ["en-US"],
    test() {
      Assert.deepEqual(
        TranslationsParent.getPreferredLanguages(),
        ["en"],
        "When all locales are English, only English is preferred."
      );
    },
  });

  await testWithLocales({
    systemLocales: ["es-ES"],
    appLocales: ["en-US"],
    webLanguages: ["en-US"],
    test() {
      Assert.deepEqual(
        TranslationsParent.getPreferredLanguages(),
        ["en", "es"],
        "When the operating system differs, it is added tot he end of the preferred languages."
      );
    },
  });

  await testWithLocales({
    systemLocales: ["zh-TW", "zh-CN", "de"],
    appLocales: ["pt-BR", "pl"],
    webLanguages: ["cs", "hu"],
    test() {
      Assert.deepEqual(
        TranslationsParent.getPreferredLanguages(),
        [
          // webLanguages
          "cs",
          "hu",
          // appLocales, notice that "en" is the last fallback.
          "pt",
          "pl",
          "en",
          // systemLocales
          "zh",
          "de",
        ],
        "Demonstrate an unrealistic but complicated locale situation."
      );
    },
  });
});
