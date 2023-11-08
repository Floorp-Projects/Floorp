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

add_setup(async function () {
  await SearchTestUtils.useTestEngines("data", null, CONFIG);
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
