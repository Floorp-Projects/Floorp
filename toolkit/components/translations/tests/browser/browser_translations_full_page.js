/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that the full page translation feature works.
 */
add_task(async function test_full_page_translation() {
  await autoTranslatePage({
    page: SPANISH_PAGE_URL,
    languagePairs: [
      { fromLang: "es", toLang: "en" },
      { fromLang: "en", toLang: "es" },
    ],
    runInPage: async TranslationsTest => {
      const selectors = TranslationsTest.getSelectors();

      await TranslationsTest.assertTranslationResult(
        "The main title gets translated.",
        selectors.getH1,
        "DON QUIJOTE DE LA MANCHA [es to en, html]"
      );

      await TranslationsTest.assertTranslationResult(
        "The last paragraph gets translated. It is out of the viewport.",
        selectors.getLastParagraph,
        "— PUES, AUNQUE MOVÁIS MÁS BRAZOS QUE LOS DEL GIGANTE BRIAREO, ME LO HABÉIS DE PAGAR. [es to en, html]"
      );

      selectors.getH1().innerText = "Este es un titulo";

      await TranslationsTest.assertTranslationResult(
        "Mutations get tracked",
        selectors.getH1,
        "ESTE ES UN TITULO [es to en]"
      );

      await TranslationsTest.assertTranslationResult(
        "Other languages do not get translated.",
        selectors.getHeader,
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

  await autoTranslatePage({
    page: ENGLISH_PAGE_URL,
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

/**
 * Check that the full page translation feature works.
 */
add_task(async function test_language_identification_for_page_translation() {
  await autoTranslatePage({
    page: NO_LANGUAGE_URL,
    languagePairs: [
      { fromLang: "es", toLang: "en" },
      { fromLang: "en", toLang: "es" },
    ],
    runInPage: async TranslationsTest => {
      const selectors = TranslationsTest.getSelectors();

      await TranslationsTest.assertTranslationResult(
        "The main title gets translated.",
        selectors.getH1,
        "DON QUIJOTE DE LA MANCHA [es to en, html]"
      );

      await TranslationsTest.assertTranslationResult(
        "The last paragraph gets translated. It is out of the viewport.",
        selectors.getLastParagraph,
        "— PUES, AUNQUE MOVÁIS MÁS BRAZOS QUE LOS DEL GIGANTE BRIAREO, ME LO HABÉIS DE PAGAR. [es to en, html]"
      );

      selectors.getH1().innerText = "Este es un titulo";

      await TranslationsTest.assertTranslationResult(
        "Mutations get tracked",
        selectors.getH1,
        "ESTE ES UN TITULO [es to en]"
      );

      await TranslationsTest.assertTranslationResult(
        "Other languages do not get translated.",
        selectors.getHeader,
        "The following is an excerpt from Don Quijote de la Mancha, which is in the public domain"
      );
    },
  });
});
