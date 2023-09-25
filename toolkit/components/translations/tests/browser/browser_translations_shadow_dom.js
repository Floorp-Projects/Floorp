/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL =
  "https://example.com/browser/toolkit/components/translations/tests/browser/translations-tester-shadow-dom-es.html";

/**
 * Check that the translation feature works with ShadowDOM.
 */
add_task(async function test_shadow_dom_translation() {
  await autoTranslatePage({
    page: URL,
    languagePairs: [
      { fromLang: "es", toLang: "en" },
      { fromLang: "en", toLang: "es" },
    ],
    runInPage: async TranslationsTest => {
      await TranslationsTest.assertTranslationResult(
        "Text outside of the Shadow DOM is translated",
        function () {
          return content.document.querySelector("h1");
        },
        "ESTO SE CONTENTA EN LUZ DOM [es to en, html]"
      );

      await TranslationsTest.assertTranslationResult(
        "The content in the Shadow DOM is translated.",
        function () {
          const root = content.document.getElementById("host").shadowRoot;
          return root.querySelector("p");
        },
        "ESTO SE CONTENTO EN SHADOW DOM [es to en, html]"
      );

      await TranslationsTest.assertTranslationResult(
        "Content in the interior root of a Shadow DOM is translated.",
        function () {
          const outerRoot = content.document.getElementById("host").shadowRoot;
          const innerRoot = outerRoot.querySelector("div").shadowRoot;
          return innerRoot.querySelector("p");
        },
        "ESTO SE CONTENTA EN RAÍZ INTERIOR [es to en, html]"
      );

      await TranslationsTest.assertTranslationResult(
        "Content in the Shaodw DOM where the host element is inside an empty textContent element is translated.",
        function () {
          const root = content.document.getElementById("host2").shadowRoot;
          return root.querySelector("p");
        },
        "ESTO SE CONTENTO EN SHADOW DOM 2 [es to en, html]"
      );
    },
  });
});
