/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_about_translations_dropdowns() {
  let languagePairs = [
    { fromLang: "en", toLang: "es" },
    { fromLang: "es", toLang: "en" },
    // This is not a bi-directional translation.
    { fromLang: "is", toLang: "en" },
  ];
  await openAboutTranslations({
    languagePairs,
    dataForContent: languagePairs,
    runInPage: async ({ dataForContent: languagePairs, selectors }) => {
      const { document } = content;

      await ContentTaskUtils.waitForCondition(
        () => {
          return document.body.hasAttribute("ready");
        },
        "Waiting for the document to be ready.",
        100,
        200
      );

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
