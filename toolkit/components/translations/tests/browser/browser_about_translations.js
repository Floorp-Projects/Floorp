/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Checks that the page renders without issue, and that the expected elements
 * are there.
 */
add_task(async function test_about_translations_enabled() {
  await openAboutTranslations({
    languagePairs: [],
    runInPage: async ({ selectors }) => {
      const { document, window } = content;

      await ContentTaskUtils.waitForCondition(() => {
        const trElement = document.querySelector(selectors.translationResult);
        const trBlankElement = document.querySelector(
          selectors.translationResultBlank
        );
        const { visibility: trVisibility } = window.getComputedStyle(trElement);
        const { visibility: trBlankVisibility } = window.getComputedStyle(
          trBlankElement
        );
        return trVisibility === "hidden" && trBlankVisibility === "visible";
      }, `Waiting for placeholder text to be visible."`);

      function checkElementIsVisible(expectVisible, name) {
        const expected = expectVisible ? "visible" : "hidden";
        const element = document.querySelector(selectors[name]);
        ok(Boolean(element), `Element ${name} was found.`);
        const { visibility } = window.getComputedStyle(element);
        is(
          visibility,
          expected,
          `Element ${name} was not ${expected} but should be.`
        );
      }

      checkElementIsVisible(true, "pageHeader");
      checkElementIsVisible(true, "fromLanguageSelect");
      checkElementIsVisible(true, "toLanguageSelect");
      checkElementIsVisible(true, "translationTextarea");
      checkElementIsVisible(true, "translationResultBlank");

      checkElementIsVisible(false, "translationResult");
    },
  });
});

/**
 * Checks that the page does not show the content when disabled.
 */
add_task(async function test_about_translations_disabled() {
  await openAboutTranslations({
    disabled: true,
    languagePairs: [],
    runInPage: async ({ selectors }) => {
      const { document, window } = content;

      await ContentTaskUtils.waitForCondition(() => {
        const element = document.querySelector(selectors.translationResult);
        const { visibility } = window.getComputedStyle(element);
        return visibility === "hidden";
      }, `Waiting for translated text to be hidden."`);

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
  let languagePairs = [
    { fromLang: "en", toLang: "es", isBeta: false },
    { fromLang: "es", toLang: "en", isBeta: false },
    // This is not a bi-directional translation.
    { fromLang: "is", toLang: "en", isBeta: true },
  ];
  await openAboutTranslations({
    languagePairs,
    dataForContent: languagePairs,
    runInPage: async ({ dataForContent: languagePairs, selectors }) => {
      const { document } = content;

      await ContentTaskUtils.waitForCondition(() => {
        return document.body.hasAttribute("ready");
      }, "Waiting for the document to be ready.");

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
        const options = [...select.options];
        const betaL10nId = "about-translations-displayname-beta";
        for (const option of options) {
          for (const languagePair of languagePairs) {
            if (
              languagePair.fromLang === option.value ||
              languagePair.toLang === option.value
            ) {
              if (option.getAttribute("data-l10n-id") === betaL10nId) {
                is(
                  languagePair.isBeta,
                  true,
                  `Since data-l10n-id was ${betaL10nId} for ${option.value}, then it must be part of a beta language pair, but it was not.`
                );
              }
              if (!languagePair.isBeta) {
                is(
                  option.getAttribute("data-l10n-id") === betaL10nId,
                  false,
                  `Since the languagePair is non-beta, the language option ${option.value} should not have a data-l10-id of ${betaL10nId}, but it does.`
                );
              }
            }
          }
        }
        info(message);
        Assert.deepEqual(
          options.filter(option => !option.hidden).map(option => option.value),
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
        message: 'From languages have "detect" already selected.',
        select: fromSelect,
        availableOptions: ["detect", "en", "is", "es"],
        selectedValue: "detect",
      });

      assertOptions({
        message:
          'The "to" options do not have "detect" in the list, and nothing is selected.',
        select: toSelect,
        availableOptions: ["", "en", "es"],
        selectedValue: "",
      });

      info('Switch the "to" language to "es".');
      toSelect.value = "es";
      toSelect.dispatchEvent(new Event("input"));

      assertOptions({
        message: 'The "from" languages no longer suggest "es".',
        select: fromSelect,
        availableOptions: ["detect", "en", "is"],
        selectedValue: "detect",
      });

      assertOptions({
        message: 'The "to" options remain the same, but "es" is selected.',
        select: toSelect,
        availableOptions: ["", "en", "es"],
        selectedValue: "es",
      });

      info('Switch the "from" language to English.');
      fromSelect.value = "en";
      fromSelect.dispatchEvent(new Event("input"));

      assertOptions({
        message: 'The "to" languages no longer suggest "en".',
        select: toSelect,
        availableOptions: ["", "es"],
        selectedValue: "es",
      });

      assertOptions({
        message: 'The "from" options remain the same, but "en" is selected.',
        select: fromSelect,
        availableOptions: ["detect", "en", "is"],
        selectedValue: "en",
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
      { fromLang: "en", toLang: "fr", isBeta: false },
      { fromLang: "fr", toLang: "en", isBeta: false },
      // This is not a bi-directional translation.
      { fromLang: "is", toLang: "en", isBeta: true },
    ],
    runInPage: async ({ selectors }) => {
      const { document, window } = content;
      Cu.waiveXrays(window).DEBOUNCE_DELAY = 5; // Make the timer run faster for tests.

      await ContentTaskUtils.waitForCondition(() => {
        return document.body.hasAttribute("ready");
      }, "Waiting for the document to be ready.");

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

add_task(async function test_about_translations_language_directions() {
  await openAboutTranslations({
    languagePairs: [
      // English (en) is LTR and Arabic (ar) is RTL.
      { fromLang: "en", toLang: "ar", isBeta: true },
      { fromLang: "ar", toLang: "en", isBeta: true },
    ],
    runInPage: async ({ selectors }) => {
      const { document, window } = content;
      Cu.waiveXrays(window).DEBOUNCE_DELAY = 5; // Make the timer run faster for tests.

      await ContentTaskUtils.waitForCondition(() => {
        return document.body.hasAttribute("ready");
      }, "Waiting for the document to be ready.");

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

/**
 * Test the debounce behavior.
 */
add_task(async function test_about_translations_debounce() {
  await openAboutTranslations({
    languagePairs: [
      { fromLang: "en", toLang: "fr", isBeta: false },
      { fromLang: "fr", toLang: "en", isBeta: false },
    ],
    runInPage: async ({ selectors }) => {
      const { document, window } = content;
      // Do not allow the debounce to come to completion.
      Cu.waiveXrays(window).DEBOUNCE_DELAY = 5;

      await ContentTaskUtils.waitForCondition(() => {
        return document.body.hasAttribute("ready");
      }, "Waiting for the document to be ready.");

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

      function setInput(element, value) {
        element.value = value;
        element.dispatchEvent(new Event("input"));
      }
      setInput(fromSelect, "en");
      setInput(toSelect, "fr");
      setInput(translationTextarea, "T");

      info("Get the translations into a stable translationed state");
      await assertTranslationResult("T [en to fr]");

      info("Reset and pause the debounce state.");
      Cu.waiveXrays(window).DEBOUNCE_DELAY = 1_000_000_000;
      Cu.waiveXrays(window).DEBOUNCE_RUN_COUNT = 0;

      info("Input text which will be debounced.");
      setInput(translationTextarea, "T");
      setInput(translationTextarea, "Te");
      setInput(translationTextarea, "Tex");
      is(Cu.waiveXrays(window).DEBOUNCE_RUN_COUNT, 0, "Debounce has not run.");

      info("Allow the debounce to actually come to completion.");
      Cu.waiveXrays(window).DEBOUNCE_DELAY = 5;
      setInput(translationTextarea, "Text");

      await assertTranslationResult("TEXT [en to fr]");
      is(Cu.waiveXrays(window).DEBOUNCE_RUN_COUNT, 1, "Debounce ran once.");
    },
  });
});

/**
 * Test the useHTML pref.
 */
add_task(async function test_about_translations_html() {
  await openAboutTranslations({
    languagePairs: [
      { fromLang: "en", toLang: "fr", isBeta: false },
      { fromLang: "fr", toLang: "en", isBeta: false },
    ],
    prefs: [["browser.translations.useHTML", true]],
    runInPage: async ({ selectors }) => {
      const { document, window } = content;
      Cu.waiveXrays(window).DEBOUNCE_DELAY = 5; // Make the timer run faster for tests.

      await ContentTaskUtils.waitForCondition(() => {
        return document.body.hasAttribute("ready");
      }, "Waiting for the document to be ready.");

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
    detectedLanguageLabel: "en",
    detectedLanguageConfidence: "0.98",
    languagePairs: [
      { fromLang: "en", toLang: "fr", isBeta: false },
      { fromLang: "fr", toLang: "en", isBeta: false },
    ],
    runInPage: async ({ selectors }) => {
      const { document, window } = content;
      Cu.waiveXrays(window).DEBOUNCE_DELAY = 5; // Make the timer run faster for tests.

      await ContentTaskUtils.waitForCondition(() => {
        return document.body.hasAttribute("ready");
      }, "Waiting for the document to be ready.");

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
          "The language identification engine correctly informs the translation."
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

      await ContentTaskUtils.waitForCondition(() => {
        const element = document.querySelector(
          selectors.translationResultBlank
        );
        const { visibility } = window.getComputedStyle(element);
        return visibility === "hidden";
      }, `Waiting for placeholder text to be visible."`);

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

/**
 * Test that the page is properly disabled when the engine isn't supported.
 */
add_task(async function test_about_translations_() {
  await openAboutTranslations({
    prefs: [["browser.translations.simulateUnsupportedEngine", true]],
    runInPage: async ({ selectors }) => {
      const { document, window } = content;

      info('Checking for the "no support" message.');
      await ContentTaskUtils.waitForCondition(
        () => document.querySelector(selectors.noSupportMessage),
        'Waiting for the "no support" message.'
      );

      /** @type {HTMLSelectElement} */
      const fromSelect = document.querySelector(selectors.fromLanguageSelect);
      /** @type {HTMLSelectElement} */
      const toSelect = document.querySelector(selectors.toLanguageSelect);
      /** @type {HTMLTextAreaElement} */
      const translationTextarea = document.querySelector(
        selectors.translationTextarea
      );

      ok(fromSelect.disabled, "The from select is disabled");
      ok(toSelect.disabled, "The to select is disabled");
      ok(translationTextarea.disabled, "The textarea is disabled");

      function checkElementIsVisible(expectVisible, name) {
        const expected = expectVisible ? "visible" : "hidden";
        const element = document.querySelector(selectors[name]);
        ok(Boolean(element), `Element ${name} was found.`);
        const { visibility } = window.getComputedStyle(element);
        is(
          visibility,
          expected,
          `Element ${name} was not ${expected} but should be.`
        );
      }

      checkElementIsVisible(true, "translationInfo");
    },
  });
});
