/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Checks that the page renders without issue, and that the expected elements
 * are there.
 */
add_task(async function test_about_translations_enabled() {
  await openAboutTranslations({
    runInPage: async ({ selectors }) => {
      const { document, window } = content;

      function checkElementIsVisble(name) {
        const element = document.querySelector(selectors[name]);
        ok(Boolean(element), `Element ${name} was found.`);
        const { visibility } = window.getComputedStyle(element);
        is(visibility, "visible", `Element ${name} was visible.`);
      }

      checkElementIsVisble("pageHeader");
      checkElementIsVisble("fromLanguageSelect");
      checkElementIsVisble("toLanguageSelect");
      checkElementIsVisble("translationTextarea");
      checkElementIsVisble("translationResult");
      checkElementIsVisble("translationResultBlank");
    },
  });
});

/**
 * Checks that the page does not show the content when disabled.
 */
add_task(async function test_about_translations_disabled() {
  await openAboutTranslations({
    disabled: true,

    runInPage: async ({ selectors }) => {
      const { document, window } = content;

      function checkElementIsInvisible(name) {
        const element = document.querySelector(selectors[name]);
        ok(Boolean(element), `Element ${name} was found.`);
        const { visibility } = window.getComputedStyle(element);
        is(visibility, "hidden", `Element ${name} was invisible.`);
      }

      checkElementIsInvisible("pageHeader");
      checkElementIsInvisible("fromLanguageSelect");
      checkElementIsInvisible("toLanguageSelect");
      checkElementIsInvisible("translationTextarea");
      checkElementIsInvisible("translationResult");
      checkElementIsInvisible("translationResultBlank");
    },
  });
});

add_task(async function test_about_translations_dropdowns() {
  const { appLocaleAsBCP47 } = Services.locale;
  if (!appLocaleAsBCP47.startsWith("en")) {
    console.warn(
      "This test assumes to be running in an 'en' app locale, however the app locale " +
        `is set to ${appLocaleAsBCP47}. Skipping the test.`
    );
    ok(true, "Skipping test.");
    return;
  }

  await openAboutTranslations({
    languagePairs: [
      { fromLang: "en", toLang: "es" },
      { fromLang: "es", toLang: "en" },
      // This is not a bi-directional translation.
      { fromLang: "is", toLang: "en" },
    ],
    runInPage: async ({ selectors }) => {
      const { document } = content;

      /**
       * Some languages can be marked as hidden in the dropbdown. This function
       * asserts the configuration of the options.
       *
       * @param {object} args
       * @param {string} args.message
       * @param {HTMLSelectElement} args.select
       * @param {string[]} args.availableOptions
       * @param {string} args.selectedValue
       */
      function assertOptions({
        message,
        select,
        availableOptions,
        selectedValue,
      }) {
        const options = [...select.options]
          .filter(option => !option.hidden)
          .map(option => option.value);

        info(message);
        Assert.deepEqual(
          options,
          availableOptions,
          "The available options match."
        );
        is(selectedValue, select.value, "The selected value matches.");
      }

      /** @type {HTMLSelectElement} */
      const fromSelect = document.querySelector(selectors.fromLanguageSelect);
      /** @type {HTMLSelectElement} */
      const toSelect = document.querySelector(selectors.toLanguageSelect);

      assertOptions({
        message: "From languages have English already selected.",
        select: fromSelect,
        availableOptions: ["", "en", "is", "es"],
        selectedValue: "en",
      });

      assertOptions({
        message:
          'The "to" options do not have "en" in the list, and nothing is selected.',
        select: toSelect,
        availableOptions: ["", "is", "es"],
        selectedValue: "",
      });

      info('Switch the "to" language to Spanish.');
      toSelect.value = "es";
      toSelect.dispatchEvent(new Event("input"));

      assertOptions({
        message: 'The "from" languages no longer suggest Spanish.',
        select: fromSelect,
        availableOptions: ["", "en", "is"],
        selectedValue: "en",
      });

      assertOptions({
        message: 'The "to" options remain the same.',
        select: toSelect,
        availableOptions: ["", "is", "es"],
        selectedValue: "es",
      });
    },
  });
});

/**
 * Test that the UI actually translates text, but use a mocked translations engine.
 * The results of the "translation" will be modifying the text to be full width latin
 * characters, so that the results visually appear modified.
 */
add_task(async function test_about_translations_translations() {
  await openAboutTranslations({
    languagePairs: [
      { fromLang: "en", toLang: "fr" },
      { fromLang: "fr", toLang: "en" },
      // This is not a bi-directional translation.
      { fromLang: "is", toLang: "en" },
    ],
    runInPage: async ({ selectors }) => {
      const { document } = content;

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

      async function assertTranslationResult(translation) {
        try {
          await ContentTaskUtils.waitForCondition(
            () => translation === translationResult.innerText,
            `Waiting for: "${translation}"`
          );
        } catch (error) {
          // The result wasn't found, but the assertion below will report the error.
          console.error(error);
        }

        is(
          translation,
          translationResult.innerText,
          "The text runs through the mocked translations engine."
        );
      }

      toSelect.value = "fr";
      toSelect.dispatchEvent(new Event("input"));
      translationTextarea.value = "Text to translate.";
      translationTextarea.dispatchEvent(new Event("input"));

      // The mocked translations make the text uppercase and reports the models used.
      await assertTranslationResult("TEXT TO TRANSLATE. [en to fr]");

      // Blank out the "to" select so it doesn't try to translate between is to fr.
      toSelect.value = "";
      toSelect.dispatchEvent(new Event("input"));

      fromSelect.value = "is";
      fromSelect.dispatchEvent(new Event("input"));
      toSelect.value = "en";
      toSelect.dispatchEvent(new Event("input"));
      translationTextarea.value = "This is the second translation.";
      translationTextarea.dispatchEvent(new Event("input"));

      await assertTranslationResult(
        "THIS IS THE SECOND TRANSLATION. [is to en]"
      );
    },
  });
});

add_task(async function test_about_translations_language_directions() {
  await openAboutTranslations({
    languagePairs: [
      // English (en) is LTR and Arabic (ar) is RTL.
      { fromLang: "en", toLang: "ar" },
      { fromLang: "ar", toLang: "en" },
    ],
    runInPage: async ({ selectors }) => {
      const { document, window } = content;

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
