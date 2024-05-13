/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests reloading engines when changing the in-use locale of a WebExtension,
 * where the name of the engine changes as well.
 */

"use strict";

const CONFIG = [
  {
    webExtension: {
      id: "engine@search.mozilla.org",
      name: "Test search engine",
      search_url: "https://www.google.com/search",
      params: [
        {
          name: "q",
          value: "{searchTerms}",
        },
        {
          name: "channel",
          condition: "purpose",
          purpose: "contextmenu",
          value: "rcs",
        },
        {
          name: "channel",
          condition: "purpose",
          purpose: "keyword",
          value: "fflb",
        },
      ],
      suggest_url:
        "https://suggestqueries.google.com/complete/search?output=firefox&client=firefox&q={searchTerms}",
    },
    appliesTo: [
      {
        included: { everywhere: true },
        default: "yes",
      },
    ],
  },
  {
    webExtension: {
      id: "engine-diff-name@search.mozilla.org",
      default_locale: "en",
      searchProvider: {
        en: {
          name: "engine-diff-name-en",
          search_url: "https://en.wikipedia.com/search",
        },
        gd: {
          name: "engine-diff-name-gd",
          search_url: "https://gd.wikipedia.com/search",
        },
      },
    },
    appliesTo: [
      {
        included: { everywhere: true },
        excluded: { locales: { matches: ["gd"] } },
      },
      {
        included: { locales: { matches: ["gd"] } },
        webExtension: {
          locales: ["gd"],
        },
      },
    ],
  },
];

const CONFIG_V2 = [
  {
    recordType: "engine",
    identifier: "engine",
    base: {
      name: "Test search engine",
      urls: {
        search: {
          base: "https://www.google.com/search",
          params: [
            {
              name: "channel",
              searchAccessPoint: {
                addressbar: "fflb",
                contextmenu: "rcs",
              },
            },
          ],
          searchTermParamName: "q",
        },
        suggestions: {
          base: "https://suggestqueries.google.com/complete/search?output=firefox&client=firefox",
          searchTermParamName: "q",
        },
      },
    },
    variants: [
      {
        environment: { allRegionsAndLocales: true },
      },
    ],
  },
  {
    recordType: "engine",
    identifier: "engine-diff-name-en",
    base: {
      name: "engine-diff-name-en",
      urls: {
        search: {
          base: "https://en.wikipedia.com/search",
          searchTermParamName: "q",
        },
      },
    },
    variants: [
      {
        environment: { excludedLocales: ["gd"] },
      },
    ],
  },
  {
    recordType: "engine",
    identifier: "engine-diff-name-gd",
    base: {
      name: "engine-diff-name-gd",
      urls: {
        search: {
          base: "https://gd.wikipedia.com/search",
          searchTermParamName: "q",
        },
      },
    },
    variants: [
      {
        environment: { locales: ["gd"] },
      },
    ],
  },
  {
    recordType: "defaultEngines",
    globalDefault: "engine",
    specificDefaults: [],
  },
  {
    recordType: "engineOrders",
    orders: [],
  },
];

add_setup(async () => {
  Services.locale.availableLocales = [
    ...Services.locale.availableLocales,
    "en",
    "gd",
  ];
  Services.locale.requestedLocales = ["gd"];

  await SearchTestUtils.useTestEngines(
    "data",
    null,
    SearchUtils.newSearchConfigEnabled ? CONFIG_V2 : CONFIG
  );
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

add_task(async function test_config_updated_engine_changes() {
  let engines = await Services.search.getEngines();
  Assert.deepEqual(
    engines.map(e => e.name),
    ["Test search engine", "engine-diff-name-gd"],
    "Should have the correct engines installed"
  );

  let engine = await Services.search.getEngineByName("engine-diff-name-gd");
  Assert.equal(
    engine.name,
    "engine-diff-name-gd",
    "Should have the correct engine name"
  );
  Assert.equal(
    engine.getSubmission("test").uri.spec,
    SearchUtils.newSearchConfigEnabled
      ? "https://gd.wikipedia.com/search?q=test"
      : "https://gd.wikipedia.com/search",
    "Should have the gd search url"
  );

  await promiseSetLocale("en");

  engines = await Services.search.getEngines();
  Assert.deepEqual(
    engines.map(e => e.name),
    ["Test search engine", "engine-diff-name-en"],
    "Should have the correct engines installed after locale change"
  );

  engine = await Services.search.getEngineByName("engine-diff-name-en");
  Assert.equal(
    engine.name,
    "engine-diff-name-en",
    "Should have the correct engine name"
  );
  Assert.equal(
    engine.getSubmission("test").uri.spec,
    SearchUtils.newSearchConfigEnabled
      ? "https://en.wikipedia.com/search?q=test"
      : "https://en.wikipedia.com/search",
    "Should have the en search url"
  );
});
