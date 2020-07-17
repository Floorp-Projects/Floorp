/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SearchEngineSelector } = ChromeUtils.import(
  "resource://gre/modules/SearchEngineSelector.jsm"
);
const { MockRegistrar } = ChromeUtils.import(
  "resource://testing-common/MockRegistrar.jsm"
);

const SEARCH_SERVICE_TOPIC = "browser-search-service";
const SEARCH_ENGINE_TOPIC = "browser-search-engine-modified";

const CONFIG = [
  {
    webExtension: {
      id: "engine@search.mozilla.org",
    },
    orderHint: 30,
    appliesTo: [
      {
        included: { everywhere: true },
        excluded: { regions: ["FR"] },
        default: "yes",
        defaultPrivate: "yes",
      },
    ],
  },
  {
    webExtension: {
      id: "engine-pref@search.mozilla.org",
    },
    orderHint: 20,
    appliesTo: [
      {
        included: { regions: ["FR"] },
        default: "yes",
        defaultPrivate: "yes",
      },
    ],
  },
];

const ALTERNATE_CONFIG = [
  {
    webExtension: {
      id: "engine-pref@search.mozilla.org",
    },
    orderHint: 20,
    appliesTo: [
      {
        included: { regions: ["FR"] },
        default: "no",
        defaultPrivate: "no",
      },
    ],
  },
  {
    webExtension: {
      id: "engine-chromeicon@search.mozilla.org",
    },
    orderHint: 20,
    appliesTo: [
      {
        included: { regions: ["FR"] },
        default: "yes",
        defaultPrivate: "no",
        extraParams: [
          { name: "c", value: "my-test" },
          { name: "q1", value: "{searchTerms}" },
        ],
      },
    ],
  },
  {
    webExtension: {
      id: "engine-resourceicon@search.mozilla.org",
    },
    orderHint: 20,
    appliesTo: [
      {
        included: { regions: ["FR"] },
        default: "no",
        defaultPrivate: "yes",
      },
    ],
  },
];

// Default engine with no region defined.
const DEFAULT = "Test search engine";
// Default engine with region set to FR.
const FR_DEFAULT = "engine-pref";

function listenFor(name, key) {
  let notifyObserved = false;
  let obs = (subject, topic, data) => {
    if (data == key) {
      notifyObserved = true;
    }
  };
  Services.obs.addObserver(obs, name);

  return () => {
    Services.obs.removeObserver(obs, name);
    return notifyObserved;
  };
}

add_task(async function setup() {
  Services.prefs.setBoolPref("browser.search.separatePrivateDefault", true);
  Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", true);
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );

  SearchTestUtils.useMockIdleService(registerCleanupFunction);
  await useTestEngines("data", null, CONFIG);
  await AddonTestUtils.promiseStartupManager();

  Region._setHomeRegion("", false);

  await Services.search.init();
  // Prime the default engines, so the SearchService properties are primed.
  await Services.search.getDefaultPrivate();
  await Services.search.getDefault();
});

add_task(async function test_config_updated_engine_changes() {
  // Update the config.
  const reloadObserved = SearchTestUtils.promiseSearchNotification(
    "engines-reloaded"
  );
  const defaultEngineChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.DEFAULT,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  const defaultPrivateEngineChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );

  const enginesAdded = [];

  function enginesAddedObs(subject, topic, data) {
    if (data != SearchUtils.MODIFIED_TYPE.ADDED) {
      return;
    }

    enginesAdded.push(subject.QueryInterface(Ci.nsISearchEngine).identifier);
  }
  Services.obs.addObserver(enginesAddedObs, SearchUtils.TOPIC_ENGINE_MODIFIED);

  Region._setHomeRegion("FR", false);

  await RemoteSettings(SearchUtils.SETTINGS_KEY).emit("sync", {
    data: {
      current: ALTERNATE_CONFIG,
    },
  });

  SearchTestUtils.idleService._fireObservers("idle");

  await reloadObserved;
  Services.obs.removeObserver(
    enginesAddedObs,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );

  Assert.deepEqual(
    enginesAdded.sort(),
    ["engine-chromeicon", "engine-pref", "engine-resourceicon"],
    "Should have installed the correct engines"
  );

  const installedEngines = await Services.search.getDefaultEngines();

  Assert.deepEqual(
    installedEngines.map(e => e.identifier),
    // TODO: engine shouldn't be in this list, since it is not in the new
    // configuration.
    ["engine-chromeicon", "engine-resourceicon", "engine", "engine-pref"],
    "Should have the correct list of engines installed."
  );

  const newDefault = await defaultEngineChanged;
  Assert.equal(
    newDefault.identifier,
    "engine-chromeicon",
    "Should have correctly notified the new default engine"
  );

  const newDefaultPrivate = await defaultPrivateEngineChanged;
  Assert.equal(
    newDefaultPrivate.identifier,
    "engine-resourceicon",
    "Should have correctly notified the new default private engine"
  );

  const engineWithParams = await Services.search.getEngineByName(
    "engine-chromeicon"
  );
  Assert.equal(
    engineWithParams.getSubmission("test").uri.spec,
    "https://www.google.com/search?c=my-test&q1=test",
    "Should have updated the parameters"
  );
});
