/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

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
    appliesTo: [{ included: { everywhere: true } }],
  },
  {
    // This engine has the same name, but still should be replaced correctly.
    webExtension: {
      id: "engine-same-name@search.mozilla.org",
      default_locale: "en",
      searchProvider: {
        en: {
          name: "engine-same-name",
          search_url: "https://www.google.com/search?q={searchTerms}",
        },
        gd: {
          name: "engine-same-name",
          search_url: "https://www.example.com/search?q={searchTerms}",
        },
      },
    },
    appliesTo: [
      {
        included: { everywhere: true },
        excluded: { regions: ["FR"] },
      },
      {
        included: { regions: ["FR"] },
        webExtension: {
          locales: ["gd"],
        },
      },
    ],
  },
];

add_setup(async function () {
  await SearchTestUtils.useTestEngines("data", null, CONFIG);
  await AddonTestUtils.promiseStartupManager();
  let settingsFileWritten = promiseAfterSettings();
  Region._setHomeRegion("US", false);
  await Services.search.init();
  await settingsFileWritten;
});

add_task(async function test_settings_persist_diff_locale_same_name() {
  let settingsFileWritten = promiseAfterSettings();
  let engine1 = Services.search.getEngineByName("engine-same-name");
  Services.search.moveEngine(engine1, 0);

  let engine2 = Services.search.getEngineByName("Test search engine");
  Services.search.moveEngine(engine2, 1);
  // Ensure we have saved the settings before we restart below.
  await settingsFileWritten;

  Assert.deepEqual(
    (await Services.search.getEngines()).map(e => e.name),
    ["engine-same-name", "Test search engine"],
    "Should have set the engines to the expected order"
  );

  // Setting the region to FR will change the engine id, but use the same name.
  Region._setHomeRegion("FR", false);

  // Pretend we are restarting.
  Services.search.wrappedJSObject.reset();
  await Services.search.init();

  Assert.deepEqual(
    (await Services.search.getEngines()).map(e => e.name),
    ["engine-same-name", "Test search engine"],
    "Should have retained the engines in the expected order"
  );
});
