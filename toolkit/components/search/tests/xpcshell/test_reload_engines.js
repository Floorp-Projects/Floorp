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

// The mock idle service.
var idleService = {
  _observers: new Set(),

  _reset() {
    this._observers.clear();
  },

  _fireObservers(state) {
    for (let observer of this._observers.values()) {
      observer.observe(observer, state, null);
    }
  },

  QueryInterface: ChromeUtils.generateQI([Ci.nsIIdleService]),
  idleTime: 19999,

  addIdleObserver(observer, time) {
    this._observers.add(observer);
  },

  removeIdleObserver(observer, time) {
    this._observers.delete(observer);
  },
};

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

  let fakeIdleService = MockRegistrar.register(
    "@mozilla.org/widget/idleservice;1",
    idleService
  );
  registerCleanupFunction(() => {
    MockRegistrar.unregister(fakeIdleService);
  });

  await useTestEngines("data", null, CONFIG);
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_regular_init() {
  let reloadObserved = listenFor(SEARCH_SERVICE_TOPIC, "engines-reloaded");
  let geoUrl = `data:application/json,{"country_code": "FR"}`;
  Services.prefs.setCharPref("browser.region.network.url", geoUrl);

  await Promise.all([
    Services.search.init(true),
    SearchTestUtils.promiseSearchNotification("ensure-known-region-done"),
    promiseAfterCache(),
  ]);

  Assert.equal(
    Services.search.defaultEngine.name,
    FR_DEFAULT,
    "Geo defined default should be set"
  );

  Assert.ok(
    !reloadObserved(),
    "Engines don't reload with immediate region fetch"
  );
});

add_task(async function test_init_with_slow_region_lookup() {
  let reloadObserved = listenFor(SEARCH_SERVICE_TOPIC, "engines-reloaded");
  let initPromise;

  // Ensure the region lookup completes after init so the
  // engines are reloaded
  Services.prefs.setCharPref("browser.search.region", "");
  let srv = useHttpServer();
  srv.registerPathHandler("/fetch_region", async (req, res) => {
    res.processAsync();
    await initPromise;
    res.setStatusLine("1.1", 200, "OK");
    res.write(JSON.stringify({ country_code: "FR" }));
    res.finish();
  });

  Services.prefs.setCharPref(
    "browser.region.network.url",
    `http://localhost:${srv.identity.primaryPort}/fetch_region`
  );

  // Kick off a re-init.
  initPromise = asyncReInit();
  await initPromise;

  let otherPromises = [
    SearchTestUtils.promiseSearchNotification("ensure-known-region-done"),
    promiseAfterCache(),
    SearchTestUtils.promiseSearchNotification(
      "engine-default",
      SEARCH_ENGINE_TOPIC
    ),
  ];

  Assert.equal(
    Services.search.defaultEngine.name,
    DEFAULT,
    "Test engine shouldn't be the default anymore"
  );

  await Promise.all(otherPromises);

  // Ensure that correct engine is being reported as the default.
  Assert.equal(
    Services.search.defaultEngine.name,
    FR_DEFAULT,
    "engine-pref should be the default in FR"
  );
  Assert.equal(
    (await Services.search.getDefaultPrivate()).name,
    FR_DEFAULT,
    "engine-pref should be the private default in FR"
  );

  Assert.ok(reloadObserved(), "Engines do reload with delayed region fetch");
});

add_task(async function test_config_updated_engine_changes() {
  // Note: The region here is FR.

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

  await RemoteSettings(SearchUtils.SETTINGS_KEY).emit("sync", {
    data: {
      current: ALTERNATE_CONFIG,
    },
  });

  idleService._fireObservers("idle");

  await reloadObserved;
  Services.obs.removeObserver(
    enginesAddedObs,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );

  Assert.deepEqual(
    enginesAdded.sort(),
    ["engine-chromeicon", "engine-resourceicon"],
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
