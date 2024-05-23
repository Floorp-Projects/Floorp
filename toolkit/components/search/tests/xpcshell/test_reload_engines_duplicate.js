/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests reloading engines when a user has an engine installed that is the
 * same name as an application provided engine being added to the user's set
 * of engines.
 *
 * Ensures that settings are not automatically taken across.
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
      ],
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
      id: "engine-pref@search.mozilla.org",
      name: "engine-pref",
      search_url: "https://www.google.com/search",
      params: [
        {
          name: "q",
          value: "{searchTerms}",
        },
      ],
    },
    appliesTo: [
      {
        included: { regions: ["FR"] },
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
          params: [],
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
    identifier: "engine-pref",
    base: {
      name: "engine-pref",
      urls: {
        search: {
          base: "https://www.google.com/search",
          params: [],
          searchTermParamName: "q",
        },
      },
    },
    variants: [
      {
        environment: { regions: ["FR"] },
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
  let server = useHttpServer();
  server.registerContentType("sjs", "sjs");

  // We use a region that doesn't install `engine-pref` by default so that we can
  // manually install it first (like when a user installs a browser add-on), and
  // then test what happens when we switch regions to one which would install
  // `engine-pref`.
  Region._setHomeRegion("US", false);

  await SearchTestUtils.useTestEngines(
    "data",
    null,
    SearchUtils.newSearchConfigEnabled ? CONFIG_V2 : CONFIG
  );
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

add_task(async function test_reload_engines_with_duplicate() {
  let engines = await Services.search.getEngines();

  Assert.deepEqual(
    engines.map(e => e.name),
    ["Test search engine"],
    "Should have the expected default engines"
  );
  // Simulate a user installing a search engine that shares the same name as an
  // application provided search engine not currently installed in their browser.
  let engine = await SearchTestUtils.installOpenSearchEngine({
    url: `${gDataUrl}engineMaker.sjs?${JSON.stringify({
      baseURL: gDataUrl,
      name: "engine-pref",
      method: "GET",
    })}`,
  });

  engine.alias = "testEngine";

  let engineId = engine.id;

  Region._setHomeRegion("FR", false);

  await Services.search.wrappedJSObject._maybeReloadEngines();

  Assert.ok(
    !(await Services.search.getEngineById(engineId)),
    "Should not have added the duplicate engine"
  );

  engines = await Services.search.getEngines();

  Assert.deepEqual(
    engines.map(e => e.name),
    ["Test search engine", "engine-pref"],
    "Should have the expected default engines"
  );

  let enginePref = await Services.search.getEngineByName("engine-pref");

  Assert.equal(
    enginePref.alias,
    "",
    "Should not have copied the alias from the duplicate engine"
  );
});
