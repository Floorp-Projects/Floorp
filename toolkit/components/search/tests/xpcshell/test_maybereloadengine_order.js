/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_CONFIG = [
  {
    webExtension: {
      id: "plainengine@search.mozilla.org",
      name: "Plain",
      search_url: "https://duckduckgo.com/",
      params: [
        {
          name: "q",
          value: "{searchTerms}",
        },
      ],
      suggest_url: "https://ac.duckduckgo.com/ac/q={searchTerms}&type=list",
    },
    appliesTo: [{ included: { everywhere: true } }],
  },
  {
    webExtension: {
      id: "special-engine@search.mozilla.org",
      name: "Special",
      search_url: "https://www.google.com/search",
      params: [
        {
          name: "q",
          value: "{searchTerms}",
        },
        {
          name: "client",
          condition: "purpose",
          purpose: "keyword",
          value: "firefox-b-1-ab",
        },
        {
          name: "client",
          condition: "purpose",
          purpose: "searchbar",
          value: "firefox-b-1",
        },
      ],
      suggest_url:
        "https://www.google.com/complete/search?client=firefox&q={searchTerms}",
    },
    appliesTo: [{ default: "yes", included: { regions: ["FR"] } }],
  },
];

add_setup(async function () {
  await SearchTestUtils.useTestEngines("test-extensions", null, TEST_CONFIG);
  await AddonTestUtils.promiseStartupManager();

  registerCleanupFunction(AddonTestUtils.promiseShutdownManager);
});

add_task(async function basic_multilocale_test() {
  let resolver;
  let initPromise = new Promise(resolve => (resolver = resolve));
  useCustomGeoServer("FR", initPromise);

  await Services.search.init();
  await Services.search.getAppProvidedEngines();
  resolver();
  await SearchTestUtils.promiseSearchNotification("engines-reloaded");

  let engines = await Services.search.getAppProvidedEngines();

  Assert.deepEqual(
    engines.map(e => e._name),
    ["Special", "Plain"],
    "Special engine is default so should be first"
  );

  engines.forEach(engine => {
    Assert.ok(!engine._metaData.order, "Order is not defined");
  });

  Assert.equal(
    Services.search.wrappedJSObject._settings.getMetaDataAttribute(
      "useSavedOrder"
    ),
    false,
    "We should not set the engine order during maybeReloadEngines"
  );
});
