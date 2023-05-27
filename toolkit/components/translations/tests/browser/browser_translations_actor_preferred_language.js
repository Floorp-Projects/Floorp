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

async function mockLocales({ systemLocales, appLocales, webLanguages, test }) {
  const appLocaleChanged1 = waitForAppLocaleChanged();

  TranslationsParent.mockedSystemLocales = systemLocales;
  const { availableLocales, requestedLocales } = Services.locale;

  info("Mocking locales, so expect potential .ftl resource errors.");
  Services.locale.availableLocales = appLocales;
  Services.locale.requestedLocales = appLocales;

  await appLocaleChanged1;

  await SpecialPowers.pushPrefEnv({
    set: [["intl.accept_languages", webLanguages.join(",")]],
  });

  test();

  const appLocaleChanged2 = waitForAppLocaleChanged();

  // Reset back to the originals.
  TranslationsParent.mockedSystemLocales = null;
  Services.locale.availableLocales = availableLocales;
  Services.locale.requestedLocales = requestedLocales;

  await appLocaleChanged2;

  await SpecialPowers.popPrefEnv();
}

add_task(async function test_preferred_language() {
  await mockLocales({
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

  await mockLocales({
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

  await mockLocales({
    systemLocales: ["zh-TW", "zh-CN", "de"],
    appLocales: ["pt-BR", "pl"],
    webLanguages: ["cs", "hu"],
    test() {
      Assert.deepEqual(
        TranslationsParent.getPreferredLanguages(),
        [
          // appLocales, notice that "en" is the last fallback.
          "pt",
          "pl",
          "en",
          // webLanguages
          "cs",
          "hu",
          // systemLocales
          "zh",
          "de",
        ],
        "Demonstrate an unrealistic but complicated locale situation."
      );
    },
  });
});
