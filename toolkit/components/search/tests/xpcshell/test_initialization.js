/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that a delayed add-on manager start up does not affect the start up
// of the search service.

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
        "https://suggestqueries.google.com/complete/search?output=firefox&client=firefox&hl={moz:locale}&q={searchTerms}",
    },
    orderHint: 30,
    appliesTo: [
      {
        included: { everywhere: true },
        default: "yes",
      },
    ],
  },
];

add_setup(() => {
  do_get_profile();
  Services.fog.initializeFOG();
});

add_task(async function test_initialization_delayed_addon_manager() {
  let stub = await SearchTestUtils.useTestEngines("data", null, CONFIG);
  // Wait until the search service gets its configuration before starting
  // to initialise the add-on manager. This simulates the add-on manager
  // starting late which used to cause the search service to fail to load any
  // engines.
  stub.callsFake(() => {
    Services.tm.dispatchToMainThread(() => {
      AddonTestUtils.promiseStartupManager();
    });
    return CONFIG;
  });

  await Services.search.init();

  Assert.equal(
    Services.search.defaultEngine.name,
    "Test search engine",
    "Test engine shouldn't be the default anymore"
  );

  await assertGleanDefaultEngine({
    normal: {
      engineId: "engine",
      displayName: "Test search engine",
      loadPath: [
        SearchUtils.newSearchConfigEnabled
          ? "[app]engine@search.mozilla.org"
          : "[addon]engine@search.mozilla.org",
      ],
      submissionUrl: "https://www.google.com/search?q=",
      verified: "default",
    },
  });
});
