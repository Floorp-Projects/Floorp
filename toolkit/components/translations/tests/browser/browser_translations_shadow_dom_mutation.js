/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL =
  "https://example.com/browser/toolkit/components/translations/tests/browser/translations-tester-shadow-dom-mutation-es.html";

const URL_2 =
  "https://example.com/browser/toolkit/components/translations/tests/browser/translations-tester-shadow-dom-mutation-es-2.html";

/**
 * Check that the translation feature works with mutations around ShadowDOM
 */
add_task(async function test_shadow_dom_mutation() {
  await autoTranslatePage({
    page: URL,
    languagePairs: [
      { fromLang: "es", toLang: "en" },
      { fromLang: "en", toLang: "es" },
    ],
    runInPage: async TranslationsTest => {
      await TranslationsTest.assertTranslationResult(
        "Basic translation works",
        function () {
          return content.document.querySelector("h1");
        },
        "THIS IS CONTENT IN LIGHT DOM [es to en, html]"
      );

      info("Test 1: Mutation on existing shadow tree");
      const root1 = content.document.getElementById("host1").shadowRoot;
      root1.innerHTML = "<p>This is mutated content in the Shadow DOM</p>";
      await TranslationsTest.assertTranslationResult(
        "The content is translated when shadow tree is modified",
        function () {
          const root1 = content.document.getElementById("host1").shadowRoot;
          return root1.querySelector("p");
        },
        "THIS IS MUTATED CONTENT IN THE SHADOW DOM [es to en, html]"
      );

      info("Test 2: Shadow host added later");
      const host2 = content.document.createElement("div");
      host2.id = "host2";
      const root2 = host2.attachShadow({ mode: "open" });
      root2.innerHTML = "<p>This is content in a shadow DOM</p>";
      content.document.body.appendChild(host2);
      await TranslationsTest.assertTranslationResult(
        "The content is translated when the host element is added later",
        function () {
          const root2 = content.document.getElementById("host2").shadowRoot;
          return root2.querySelector("p");
        },
        "THIS IS CONTENT IN A SHADOW DOM [es to en, html]"
      );

      info("Test 3: Mutation Works on newly added shadow tree");
      const newNode = content.document.createElement("p");
      newNode.innerHTML =
        "<p>This is mutated content in newly added shadow DOM</p>";
      newNode.id = "newNode";
      root2.appendChild(newNode);
      await TranslationsTest.assertTranslationResult(
        "The content is translated when a new node is added to the newly added shadow tree",
        function () {
          const root2 = content.document.getElementById("host2").shadowRoot;
          return root2.getElementById("newNode");
        },
        "THIS IS MUTATED CONTENT IN NEWLY ADDED SHADOW DOM [es to en, html]"
      );
    },
  });
});

add_task(async function test_shadow_dom_mutation_nested_1() {
  await autoTranslatePage({
    page: URL,
    languagePairs: [
      { fromLang: "es", toLang: "en" },
      { fromLang: "en", toLang: "es" },
    ],
    runInPage: async TranslationsTest => {
      await TranslationsTest.assertTranslationResult(
        "Basic translation works",
        function () {
          return content.document.querySelector("h1");
        },
        "THIS IS CONTENT IN LIGHT DOM [es to en, html]"
      );

      info("Test 1: Nested shadow host added later");
      const root1 = content.document.getElementById("host1").shadowRoot;
      root1.innerHTML = "<div id='innerHost'></div>";

      const innerHost = root1.getElementById("innerHost");
      innerHost.id = "innerHost";
      const innerRoot = innerHost.attachShadow({ mode: "open" });
      innerRoot.innerHTML = "<p>This is content in nested shadow DOM</p>";

      await TranslationsTest.assertTranslationResult(
        "The content is translated when a inner host element is added later",
        function () {
          const root1 = content.document.getElementById("host1").shadowRoot;
          const innerRoot = root1.getElementById("innerHost").shadowRoot;
          return innerRoot.querySelector("p");
        },
        "THIS IS CONTENT IN NESTED SHADOW DOM [es to en, html]"
      );

      info("Test 2: Mutation on inner shadow tree works");
      const newInnerNode = content.document.createElement("p");
      newInnerNode.innerHTML =
        "<p>This is mutated content in nested shadow DOM</p>";
      newInnerNode.id = "newInnerNode";
      innerRoot.appendChild(newInnerNode);
      await TranslationsTest.assertTranslationResult(
        "The content is translated when inner shadow tree is mutated",
        function () {
          const root = content.document.getElementById("host1").shadowRoot;
          const innerRoot = root.getElementById("innerHost").shadowRoot;
          return innerRoot.getElementById("newInnerNode");
        },
        "THIS IS MUTATED CONTENT IN NESTED SHADOW DOM [es to en, html]"
      );
    },
  });
});

// Test to ensure mutations on a nested shadow tree that is
// added before pageload works.
add_task(async function test_shadow_dom_mutation_nested_2() {
  await autoTranslatePage({
    page: URL_2,
    languagePairs: [
      { fromLang: "es", toLang: "en" },
      { fromLang: "en", toLang: "es" },
    ],
    runInPage: async TranslationsTest => {
      await TranslationsTest.assertTranslationResult(
        "Basic translation works",
        function () {
          return content.document.querySelector("h1");
        },
        "THIS IS CONTENT IN LIGHT DOM [es to en, html]"
      );

      const root1 = content.document.getElementById("host1").shadowRoot;
      const innerRoot = root1.getElementById("innerHost").shadowRoot;
      innerRoot.innerHTML =
        "<p>This is mutated content in inner shadow DOM</p>";

      await TranslationsTest.assertTranslationResult(
        "The content is translated when the inner shadow tree is modified",
        function () {
          const root1 = content.document.getElementById("host1").shadowRoot;
          const innerRoot = root1.getElementById("innerHost").shadowRoot;
          return innerRoot.querySelector("p");
        },
        "THIS IS MUTATED CONTENT IN INNER SHADOW DOM [es to en, html]"
      );
    },
  });
});
