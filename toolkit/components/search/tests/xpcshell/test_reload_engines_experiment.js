/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CONFIG = [
  {
    // Just a basic engine that won't be changed.
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
        default: "yes",
      },
    ],
  },
  {
    // This engine will have the locale swapped when the experiment is set.
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
        webExtension: {
          locales: ["en"],
        },
      },
      {
        included: { everywhere: true },
        webExtension: {
          locales: ["gd"],
        },
        experiment: "xpcshell",
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
    identifier: "engine-same-name-en",
    base: {
      name: "engine-same-name",
      urls: {
        search: {
          base: "https://www.google.com/search",
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
    identifier: "engine-same-name-gd",
    base: {
      name: "engine-same-name",
      urls: {
        search: {
          base: "https://www.example.com/search",
          searchTermParamName: "q",
        },
      },
    },
    variants: [
      {
        environment: { allRegionsAndLocales: true, experiment: "xpcshell" },
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

add_setup(async function () {
  await SearchTestUtils.useTestEngines(
    "data",
    null,
    SearchUtils.newSearchConfigEnabled ? CONFIG_V2 : CONFIG
  );
  await AddonTestUtils.promiseStartupManager();
});

// This is to verify that the loaded configuration matches what we expect for
// the test.
add_task(async function test_initial_config_correct() {
  await Services.search.init();

  const installedEngines = await Services.search.getAppProvidedEngines();
  Assert.deepEqual(
    installedEngines.map(e => e.identifier),
    ["engine", "engine-same-name-en"],
    "Should have the correct list of engines installed."
  );

  Assert.equal(
    (await Services.search.getDefault()).identifier,
    "engine",
    "Should have loaded the expected default engine"
  );
});

add_task(async function test_config_updated_engine_changes() {
  // Update the config.
  const reloadObserved =
    SearchTestUtils.promiseSearchNotification("engines-reloaded");
  const enginesAdded = [];
  const enginesModified = [];
  const enginesRemoved = [];

  function enginesObs(subject, topic, data) {
    if (data == SearchUtils.MODIFIED_TYPE.ADDED) {
      enginesAdded.push(subject.QueryInterface(Ci.nsISearchEngine).identifier);
    } else if (data == SearchUtils.MODIFIED_TYPE.CHANGED) {
      enginesModified.push(
        subject.QueryInterface(Ci.nsISearchEngine).identifier
      );
    } else if (data == SearchUtils.MODIFIED_TYPE.REMOVED) {
      enginesRemoved.push(subject.QueryInterface(Ci.nsISearchEngine).name);
    }
  }
  Services.obs.addObserver(enginesObs, SearchUtils.TOPIC_ENGINE_MODIFIED);

  Services.prefs.setCharPref(
    SearchUtils.BROWSER_SEARCH_PREF + "experiment",
    "xpcshell"
  );

  await reloadObserved;
  Services.obs.removeObserver(enginesObs, SearchUtils.TOPIC_ENGINE_MODIFIED);

  if (SearchUtils.newSearchConfigEnabled) {
    // In the new config, engine-same-name-en and engine-same-name-gd are two
    // different engine configs and they will be treated as different engines
    // and not the same. That's the reason why the assertions are different below.
    Assert.deepEqual(
      enginesAdded,
      ["engine-same-name-gd"],
      "Should have added the correct engines"
    );

    Assert.deepEqual(
      enginesModified.sort(),
      ["engine", "engine-same-name-en"],
      "Should have modified the expected engines"
    );

    Assert.deepEqual(
      enginesRemoved,
      ["engine-same-name"],
      "Should have removed the expected engine"
    );
  } else {
    Assert.deepEqual(enginesAdded, [], "Should have added the correct engines");

    Assert.deepEqual(
      enginesModified.sort(),
      ["engine", "engine-same-name-gd"],
      "Should have modified the expected engines"
    );

    Assert.deepEqual(
      enginesRemoved,
      [],
      "Should have removed the expected engine"
    );
  }

  const installedEngines = await Services.search.getAppProvidedEngines();

  Assert.deepEqual(
    installedEngines.map(e => e.identifier),
    ["engine", "engine-same-name-gd"],
    "Should have the correct list of engines installed in the expected order."
  );

  const engineWithSameName = await Services.search.getEngineByName(
    "engine-same-name"
  );
  Assert.equal(
    engineWithSameName.getSubmission("test").uri.spec,
    "https://www.example.com/search?q=test",
    "Should have correctly switched to the engine of the same name"
  );

  Assert.equal(
    Services.search.wrappedJSObject._settings.getMetaDataAttribute(
      "useSavedOrder"
    ),
    false,
    "Should not have set the useSavedOrder preference"
  );
});
