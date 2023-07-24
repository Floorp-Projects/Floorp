/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_about_translations_language_directions() {
  await openAboutTranslations({
    languagePairs: [
      // English (en) is LTR and Arabic (ar) is RTL.
      { fromLang: "en", toLang: "ar" },
      { fromLang: "ar", toLang: "en" },
    ],
    runInPage: async ({ selectors }) => {
      const { document, window } = content;
      Cu.waiveXrays(window).DEBOUNCE_DELAY = 5; // Make the timer run faster for tests.

      await ContentTaskUtils.waitForCondition(
        () => {
          return document.body.hasAttribute("ready");
        },
        "Waiting for the document to be ready.",
        100,
        200
      );

      /** @type {HTMLSelectElement} */
      const fromSelect = document.querySelector(selectors.fromLanguageSelect);
      /** @type {HTMLSelectElement} */
      const toSelect = document.querySelector(selectors.toLanguageSelect);
      /** @type {HTMLTextAreaElement} */
      const translationTextarea = document.querySelector(
        selectors.translationTextarea
      );
      /** @type {HTMLDivElement} */
      const translationResult = document.querySelector(
        selectors.translationResult
      );

      fromSelect.value = "en";
      fromSelect.dispatchEvent(new Event("input"));
      toSelect.value = "ar";
      toSelect.dispatchEvent(new Event("input"));
      translationTextarea.value = "This text starts as LTR.";
      translationTextarea.dispatchEvent(new Event("input"));

      is(
        window.getComputedStyle(translationTextarea).direction,
        "ltr",
        "The English input is LTR"
      );
      is(
        window.getComputedStyle(translationResult).direction,
        "rtl",
        "The Arabic results are RTL"
      );

      toSelect.value = "";
      toSelect.dispatchEvent(new Event("input"));
      fromSelect.value = "ar";
      fromSelect.dispatchEvent(new Event("input"));
      toSelect.value = "en";
      toSelect.dispatchEvent(new Event("input"));
      translationTextarea.value = "This text starts as RTL.";
      translationTextarea.dispatchEvent(new Event("input"));

      is(
        window.getComputedStyle(translationTextarea).direction,
        "rtl",
        "The Arabic input is RTL"
      );
      is(
        window.getComputedStyle(translationResult).direction,
        "ltr",
        "The English results are LTR"
      );
    },
  });
});
