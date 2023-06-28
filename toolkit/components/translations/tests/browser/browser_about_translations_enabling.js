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

      await ContentTaskUtils.waitForCondition(
        () => {
          const trElement = document.querySelector(selectors.translationResult);
          const trBlankElement = document.querySelector(
            selectors.translationResultBlank
          );
          const { visibility: trVisibility } =
            window.getComputedStyle(trElement);
          const { visibility: trBlankVisibility } =
            window.getComputedStyle(trBlankElement);
          return trVisibility === "hidden" && trBlankVisibility === "visible";
        },
        `Waiting for placeholder text to be visible."`,
        100,
        200
      );

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
    runInPage: async ({ selectors }) => {
      const { document, window } = content;

      await ContentTaskUtils.waitForCondition(
        () => {
          const element = document.querySelector(selectors.translationResult);
          const { visibility } = window.getComputedStyle(element);
          return visibility === "hidden";
        },
        `Waiting for translated text to be hidden."`,
        100,
        200
      );

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

/**
 * Test that the page is properly disabled when the engine isn't supported.
 */
add_task(async function test_about_translations_disabling() {
  await openAboutTranslations({
    prefs: [["browser.translations.simulateUnsupportedEngine", true]],
    runInPage: async ({ selectors }) => {
      const { document, window } = content;

      info('Checking for the "no support" message.');
      await ContentTaskUtils.waitForCondition(
        () => document.querySelector(selectors.noSupportMessage),
        'Waiting for the "no support" message.',
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
