/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test is designed to check the search service keeps working if there's
// a built-in engine missing from the configuration.

"use strict";

const GOOD_CONFIG = [
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
        "https://suggestqueries.google.com/complete/search?output=firefox&client=firefox&hl={moz:locale}&q={searchTerms}",
    },
    appliesTo: [
      {
        included: { everywhere: true },
      },
    ],
  },
];

const BAD_CONFIG = [
  ...GOOD_CONFIG,
  {
    webExtension: {
      id: "engine-missing@search.mozilla.org",
    },
    appliesTo: [
      {
        included: { everywhere: true },
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
          base: "https://suggestqueries.google.com/complete/search?output=firefox&client=firefox&hl={moz:locale}",
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
    recordType: "defaultEngines",
    specificDefaults: [],
  },
  {
    recordType: "engineOrders",
    orders: [],
  },
];

add_setup(async function () {
  SearchTestUtils.useMockIdleService();
  await AddonTestUtils.promiseStartupManager();

  // This test purposely attempts to load a missing engine.
  consoleAllowList.push(
    "Could not load engine engine-missing@search.mozilla.org"
  );
});

add_task(async function test_startup_with_missing() {
  await SearchTestUtils.useTestEngines(
    "data",
    null,
    SearchUtils.newSearchConfigEnabled ? CONFIG_V2 : BAD_CONFIG
  );

  const result = await Services.search.init();
  Assert.ok(
    Components.isSuccessCode(result),
    "Should have started the search service successfully."
  );

  const engines = await Services.search.getEngines();

  Assert.deepEqual(
    engines.map(e => e.name),
    ["Test search engine"],
    "Should have listed just the good engine"
  );
});

add_task(async function test_update_with_missing() {
  let reloadObserved =
    SearchTestUtils.promiseSearchNotification("engines-reloaded");

  await RemoteSettings(SearchUtils.SETTINGS_KEY).emit("sync", {
    data: {
      current: SearchUtils.newSearchConfigEnabled ? CONFIG_V2 : GOOD_CONFIG,
    },
  });

  SearchTestUtils.idleService._fireObservers("idle");

  await reloadObserved;

  const engines = await Services.search.getEngines();

  Assert.deepEqual(
    engines.map(e => e.name),
    ["Test search engine"],
    "Should have just the good engine"
  );

  reloadObserved =
    SearchTestUtils.promiseSearchNotification("engines-reloaded");

  await RemoteSettings(SearchUtils.SETTINGS_KEY).emit("sync", {
    data: {
      current: SearchUtils.newSearchConfigEnabled ? CONFIG_V2 : BAD_CONFIG,
    },
  });

  SearchTestUtils.idleService._fireObservers("idle");

  await reloadObserved;

  Assert.deepEqual(
    engines.map(e => e.name),
    ["Test search engine"],
    "Should still have just the good engine"
  );
});
