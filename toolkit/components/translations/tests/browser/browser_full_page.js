/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
const URL_PREFIX = "https://example.com/browser/";
const DIR_PATH = "toolkit/components/translations/tests/browser/";
const TRANSLATIONS_TESTER_EN =
  URL_PREFIX + DIR_PATH + "translations-tester-en.html";
const TRANSLATIONS_TESTER_ES =
  URL_PREFIX + DIR_PATH + "translations-tester-es.html";

/**
 * Check that the full page translation feature works.
 */
add_task(async function test_full_page_translation() {
  await loadTestPage({
    page: TRANSLATIONS_TESTER_ES,
    languagePairs: [
      { fromLang: "es", toLang: "en" },
      { fromLang: "en", toLang: "es" },
    ],
    runInPage: async () => {
      const { document } = content;

      async function assertTranslationResult(message, getNode, translation) {
        try {
          await ContentTaskUtils.waitForCondition(
            () => translation === getNode()?.innerText,
            `Waiting for: "${translation}"`
          );
        } catch (error) {
          // The result wasn't found, but the assertion below will report the error.
          console.error(error);
        }

        is(translation, getNode()?.innerText, message);
      }

      const getH1 = () => document.querySelector("h1");
      const getLastParagraph = () => document.querySelector("p:last-child");
      const getHeader = () => document.querySelector("header");

      await assertTranslationResult(
        "The main title gets translated.",
        getH1,
        "DON QUIJOTE DE LA MANCHA [es to en, html]"
      );

      await assertTranslationResult(
        "The last paragraph gets translated. It is out of the viewport.",
        getLastParagraph,
        "— PUES, AUNQUE MOVÁIS MÁS BRAZOS QUE LOS DEL GIGANTE BRIAREO, ME LO HABÉIS DE PAGAR. [es to en, html]"
      );

      getH1().innerText = "Este es un titulo";

      await assertTranslationResult(
        "Mutations get tracked",
        getH1,
        "ESTE ES UN TITULO [es to en]"
      );

      await assertTranslationResult(
        "Other languages do not get translated.",
        getHeader,
        "The following is an excerpt from Don Quijote de la Mancha, which is in the public domain"
      );
    },
  });
});

/**
 * Check that the full page translation feature doesn't translate pages in the app's
 * locale.
 */
add_task(async function test_about_translations_enabled() {
  const { appLocaleAsBCP47 } = Services.locale;
  if (!appLocaleAsBCP47.startsWith("en")) {
    console.warn(
      "This test assumes to be running in an 'en' app locale, however the app locale " +
        `is set to ${appLocaleAsBCP47}. Skipping the test.`
    );
    ok(true, "Skipping test.");
    return;
  }

  await loadTestPage({
    page: TRANSLATIONS_TESTER_EN,
    languagePairs: [
      { fromLang: "es", toLang: "en" },
      { fromLang: "en", toLang: "es" },
    ],
    runInPage: async () => {
      const { document } = content;

      for (let i = 0; i < 5; i++) {
        // There is no way to directly check the non-existence of a translation, as
        // the translations engine works async, and you can't dispatch a CustomEvent
        // to listen for translations, as this script runs after the initial translations
        // check. So resort to a setTimeout and check a few times. This relies on timing,
        // but _cannot fail_ if it's working correctly. It _will most likely fail_ if
        // this page accidentally gets translated.

        const timeout = 10;

        info("Waiting for the timeout.");
        // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
        await new Promise(resolve => setTimeout(resolve, timeout));
        is(
          document.querySelector("h1").innerText,
          `"The Wonderful Wizard of Oz" by L. Frank Baum`,
          `The page remains untranslated after ${(i + 1) * timeout}ms.`
        );
      }
    },
  });
});
