/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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

      async function assertTranslationResult(translation) {
        try {
          await ContentTaskUtils.waitForCondition(
            () => translation === translationResult.innerText,
            `Waiting for: "${translation}"`,
            100,
            200
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

      fromSelect.value = "en";
      fromSelect.dispatchEvent(new Event("input"));

      toSelect.value = "fr";
      toSelect.dispatchEvent(new Event("input"));

      translationTextarea.value = "Text to translate.";
      translationTextarea.dispatchEvent(new Event("input"));

      // The mocked translations make the text uppercase and reports the models used.
      await assertTranslationResult("TEXT TO TRANSLATE. [en to fr]");
      is(
        translationResult.getAttribute("lang"),
        "fr",
        "The result is listed as in French."
      );

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
      is(
        translationResult.getAttribute("lang"),
        "en",
        "The result is listed as in English."
      );
    },
  });
});

/**
 * Test the useHTML pref.
 */
add_task(async function test_about_translations_html() {
  await openAboutTranslations({
    languagePairs: [
      { fromLang: "en", toLang: "fr" },
      { fromLang: "fr", toLang: "en" },
    ],
    prefs: [["browser.translations.useHTML", true]],
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

      async function assertTranslationResult(translation) {
        try {
          await ContentTaskUtils.waitForCondition(
            () => translation === translationResult.innerText,
            `Waiting for: "${translation}"`,
            100,
            200
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

      fromSelect.value = "en";
      fromSelect.dispatchEvent(new Event("input"));
      toSelect.value = "fr";
      toSelect.dispatchEvent(new Event("input"));
      translationTextarea.value = "Text to translate.";
      translationTextarea.dispatchEvent(new Event("input"));

      // The mocked translations make the text uppercase and reports the models used.
      await assertTranslationResult("TEXT TO TRANSLATE. [en to fr, html]");
    },
  });
});

add_task(async function test_about_translations_language_identification() {
  await openAboutTranslations({
    languagePairs: [
      { fromLang: "en", toLang: "fr" },
      { fromLang: "fr", toLang: "en" },
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

      async function assertTranslationResult(translation) {
        try {
          await ContentTaskUtils.waitForCondition(
            () => translation === translationResult.innerText,
            `Waiting for: "${translation}"`,
            100,
            200
          );
        } catch (error) {
          // The result wasn't found, but the assertion below will report the error.
          console.error(error);
        }
        is(
          translation,
          translationResult.innerText,
          "The language identification correctly informs the translation."
        );
      }

      const fromSelectStartValue = fromSelect.value;
      const detectStartText = fromSelect.options[0].textContent;

      is(
        fromSelectStartValue,
        "detect",
        'The fromSelect starting value is "detect"'
      );

      translationTextarea.value = "Text to translate.";
      translationTextarea.dispatchEvent(new Event("input"));

      toSelect.value = "fr";
      toSelect.dispatchEvent(new Event("input"));

      await ContentTaskUtils.waitForCondition(
        () => {
          const element = document.querySelector(
            selectors.translationResultBlank
          );
          const { visibility } = window.getComputedStyle(element);
          return visibility === "hidden";
        },
        `Waiting for placeholder text to be visible."`,
        100,
        200
      );

      const fromSelectFinalValue = fromSelect.value;
      is(
        fromSelectFinalValue,
        fromSelectStartValue,
        "The fromSelect value has not changed"
      );

      // The mocked translations make the text uppercase and reports the models used.
      await assertTranslationResult("TEXT TO TRANSLATE. [en to fr]");

      const detectFinalText = fromSelect.options[0].textContent;
      is(
        true,
        detectFinalText.startsWith(detectStartText) &&
          detectFinalText.length > detectStartText.length,
        `fromSelect starting display text (${detectStartText}) should be a substring of the final text (${detectFinalText})`
      );
    },
  });
});
