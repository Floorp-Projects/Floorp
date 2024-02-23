/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests for the default engine telemetry event that can be tested via xpcshell,
 * related to changing or selecting a different configuration.
 * Other tests are typically in browser mochitests.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.sys.mjs",
});

const BASE_CONFIG = [
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
    appliesTo: [{ included: { everywhere: true } }],
    default: "yes",
  },
];

const BASE_CONFIG_V2 = [
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
    globalDefault: "engine",
    specificDefaults: [],
  },
  {
    recordType: "engineOrders",
    orders: [],
  },
];
const MAIN_CONFIG = [
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
    appliesTo: [{ included: { everywhere: true } }],
    default: "no",
  },
  {
    webExtension: {
      id: "engine-chromeicon@search.mozilla.org",
      name: "engine-chromeicon",
      search_url: "https://www.google.com/search",
      params: [
        {
          name: "q",
          value: "{searchTerms}",
        },
      ],
    },
    appliesTo: [{ included: { everywhere: true } }],
    default: "yes-if-no-other",
  },
  {
    webExtension: {
      id: "engine-fr@search.mozilla.org",
      name: "Test search engine (fr)",
      search_url: "https://www.google.fr/search",
      params: [
        {
          name: "q",
          value: "{searchTerms}",
        },
        {
          name: "ie",
          value: "iso-8859-1",
        },
        {
          name: "oe",
          value: "iso-8859-1",
        },
      ],
    },
    appliesTo: [
      { included: { everywhere: true } },
      {
        included: { locales: { matches: ["fr"] } },
        excluded: { regions: ["DE"] },
        default: "yes",
      },
    ],
    default: "no",
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
        {
          name: "code",
          condition: "pref",
          pref: "code",
        },
        {
          name: "test",
          condition: "pref",
          pref: "test",
        },
      ],
    },
    appliesTo: [
      { included: { everywhere: true } },
      { included: { regions: ["DE"] }, default: "yes" },
    ],
    default: "no",
  },
  {
    webExtension: {
      id: "engine2@search.mozilla.org",
      name: "A second test engine",
      search_url: "https://duckduckgo.com/?q={searchTerms}",
    },
    appliesTo: [
      { included: { everywhere: true } },
      { included: { everywhere: true }, experiment: "test1", default: "yes" },
    ],
    default: "no",
  },
];

const MAIN_CONFIG_V2 = [
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
    recordType: "engine",
    identifier: "engine-chromeicon",
    base: {
      name: "engine-chromeicon",
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
    identifier: "engine-fr",
    base: {
      name: "Test search engine (fr)",
      urls: {
        search: {
          base: "https://www.google.fr/search",
          params: [
            {
              name: "ie",
              value: "iso-8859-1",
            },
            {
              name: "oe",
              value: "iso-8859-1",
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
    identifier: "engine-pref",
    base: {
      name: "engine-pref",
      urls: {
        search: {
          base: "https://www.google.com/search",
          params: [
            {
              name: "code",
              experimentConfig: "code",
            },
            {
              name: "test",
              experimentConfig: "test",
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
    identifier: "engine2",
    base: {
      name: "A second test engine",
      urls: {
        search: {
          base: "https://duckduckgo.com/",
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
    globalDefault: "engine-chromeicon",
    specificDefaults: [
      {
        default: "engine-fr",
        environment: { excludedRegions: ["DE"], locales: ["fr"] },
      },
      {
        default: "engine-pref",
        environment: { regions: ["DE"] },
      },
      {
        default: "engine2",
        environment: { experiment: "test1" },
      },
    ],
  },
  {
    recordType: "engineOrders",
    orders: [],
  },
];

const testSearchEngine = {
  id: "engine",
  name: "Test search engine",
  loadPath: SearchUtils.newSearchConfigEnabled
    ? "[app]engine@search.mozilla.org"
    : "[addon]engine@search.mozilla.org",
  submissionURL: "https://www.google.com/search?q=",
};
const testChromeIconEngine = {
  id: "engine-chromeicon",
  name: "engine-chromeicon",
  loadPath: SearchUtils.newSearchConfigEnabled
    ? "[app]engine-chromeicon@search.mozilla.org"
    : "[addon]engine-chromeicon@search.mozilla.org",

  submissionURL: "https://www.google.com/search?q=",
};
const testFrEngine = {
  id: "engine-fr",
  name: "Test search engine (fr)",
  loadPath: SearchUtils.newSearchConfigEnabled
    ? "[app]engine-fr@search.mozilla.org"
    : "[addon]engine-fr@search.mozilla.org",
  submissionURL: SearchUtils.newSearchConfigEnabled
    ? "https://www.google.fr/search?ie=iso-8859-1&oe=iso-8859-1&q="
    : "https://www.google.fr/search?q=&ie=iso-8859-1&oe=iso-8859-1",
};
const testPrefEngine = {
  id: "engine-pref",
  name: "engine-pref",
  loadPath: SearchUtils.newSearchConfigEnabled
    ? "[app]engine-pref@search.mozilla.org"
    : "[addon]engine-pref@search.mozilla.org",
  submissionURL: "https://www.google.com/search?q=",
};
const testEngine2 = {
  id: "engine2",
  name: "A second test engine",
  loadPath: SearchUtils.newSearchConfigEnabled
    ? "[app]engine2@search.mozilla.org"
    : "[addon]engine2@search.mozilla.org",
  submissionURL: "https://duckduckgo.com/?q=",
};

function clearTelemetry() {
  Services.telemetry.clearEvents();
  Services.fog.testResetFOG();
}

async function checkTelemetry(
  source,
  prevEngine,
  newEngine,
  checkPrivate = false,
  additionalEventsExpected = false
) {
  // TODO Bug 1876178 - Improve engine change telemetry.
  // When we reload engines due to a config change, we update the engines as
  // they may have changed, we don't track if any attribute has actually changed
  // from previous, and so we send out an update regardless. This is why in
  // this test we test for the additional `engine-update` event that's recorded.
  // In future, we should be more specific about when to record the event and
  // so only one event is captured and not two.
  let additionalEvent = [
    {
      object: checkPrivate ? "change_private" : "change_default",
      value: "engine-update",
      extra: {
        prev_id: prevEngine?.id ?? "",
        new_id: prevEngine?.id ?? "",
        new_name: prevEngine?.name ?? "",
        new_load_path: prevEngine?.loadPath ?? "",
        // Telemetry has a limit of 80 characters.
        new_sub_url: prevEngine?.submissionURL.slice(0, 80) ?? "",
      },
    },
  ];

  TelemetryTestUtils.assertEvents(
    [
      ...(additionalEventsExpected ? additionalEvent : []),
      {
        object: checkPrivate ? "change_private" : "change_default",
        value: source,
        extra: {
          prev_id: prevEngine?.id ?? "",
          new_id: newEngine?.id ?? "",
          new_name: newEngine?.name ?? "",
          new_load_path: newEngine?.loadPath ?? "",
          // Telemetry has a limit of 80 characters.
          new_sub_url: newEngine?.submissionURL.slice(0, 80) ?? "",
        },
      },
    ],
    { category: "search", method: "engine" }
  );

  let snapshot;
  if (checkPrivate) {
    snapshot = await Glean.searchEnginePrivate.changed.testGetValue();
  } else {
    snapshot = await Glean.searchEngineDefault.changed.testGetValue();
  }

  if (additionalEventsExpected) {
    delete snapshot[0].timestamp;
    Assert.deepEqual(
      snapshot[0],
      {
        category: checkPrivate
          ? "search.engine.private"
          : "search.engine.default",
        name: "changed",
        extra: {
          change_source: "engine-update",
          previous_engine_id: prevEngine?.id ?? "",
          new_engine_id: prevEngine?.id ?? "",
          new_display_name: prevEngine?.name ?? "",
          new_load_path: prevEngine?.loadPath ?? "",
          new_submission_url: prevEngine?.submissionURL ?? "",
        },
      },
      "Should have received the correct event details"
    );
    snapshot.shift();
  }

  delete snapshot[0].timestamp;
  Assert.deepEqual(
    snapshot[0],
    {
      category: checkPrivate
        ? "search.engine.private"
        : "search.engine.default",
      name: "changed",
      extra: {
        change_source: source,
        previous_engine_id: prevEngine?.id ?? "",
        new_engine_id: newEngine?.id ?? "",
        new_display_name: newEngine?.name ?? "",
        new_load_path: newEngine?.loadPath ?? "",
        new_submission_url: newEngine?.submissionURL ?? "",
      },
    },
    "Should have received the correct event details"
  );
}

let getVariableStub;

add_setup(async () => {
  Region._setHomeRegion("US", false);
  Services.locale.availableLocales = [
    ...Services.locale.availableLocales,
    "en",
    "fr",
  ];
  Services.locale.requestedLocales = ["en"];

  sinon.spy(NimbusFeatures.searchConfiguration, "onUpdate");
  sinon.stub(NimbusFeatures.searchConfiguration, "ready").resolves();
  getVariableStub = sinon.stub(
    NimbusFeatures.searchConfiguration,
    "getVariable"
  );
  getVariableStub.returns(null);

  SearchTestUtils.useMockIdleService();
  Services.fog.initializeFOG();
  sinon.stub(
    Services.search.wrappedJSObject,
    "_showRemovalOfSearchEngineNotificationBox"
  );

  await SearchTestUtils.useTestEngines(
    "data",
    null,
    SearchUtils.newSearchConfigEnabled ? BASE_CONFIG_V2 : BASE_CONFIG
  );
  await AddonTestUtils.promiseStartupManager();

  await Services.search.init();
});

add_task(async function test_configuration_changes_default() {
  clearTelemetry();

  await SearchTestUtils.updateRemoteSettingsConfig(
    SearchUtils.newSearchConfigEnabled ? MAIN_CONFIG_V2 : MAIN_CONFIG
  );

  await checkTelemetry(
    "config",
    testSearchEngine,
    testChromeIconEngine,
    false,
    true
  );
});

add_task(async function test_experiment_changes_default() {
  clearTelemetry();

  let reloadObserved =
    SearchTestUtils.promiseSearchNotification("engines-reloaded");
  getVariableStub.callsFake(name => (name == "experiment" ? "test1" : null));
  NimbusFeatures.searchConfiguration.onUpdate.firstCall.args[0]();
  await reloadObserved;

  await checkTelemetry(
    "experiment",
    testChromeIconEngine,
    testEngine2,
    false,
    true
  );

  // Reset the stub so that we are no longer in an experiment.
  getVariableStub.returns(null);
});

add_task(async function test_locale_changes_default() {
  clearTelemetry();

  let reloadObserved =
    SearchTestUtils.promiseSearchNotification("engines-reloaded");
  Services.locale.requestedLocales = ["fr"];
  await reloadObserved;

  await checkTelemetry("locale", testEngine2, testFrEngine, false, true);
});

add_task(async function test_region_changes_default() {
  clearTelemetry();

  let reloadObserved =
    SearchTestUtils.promiseSearchNotification("engines-reloaded");
  Region._setHomeRegion("DE", true);
  await reloadObserved;

  await checkTelemetry("region", testFrEngine, testPrefEngine, false, true);
});

add_task(async function test_user_changes_separate_private_pref() {
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );

  await Services.search.setDefaultPrivate(
    Services.search.getEngineByName("engine-chromeicon"),
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  Assert.notEqual(
    await Services.search.getDefault(),
    await Services.search.getDefaultPrivate(),
    "Should have different engines for the pre-condition"
  );

  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    false
  );

  clearTelemetry();

  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    false
  );

  await checkTelemetry("user_private_split", testChromeIconEngine, null, true);

  getVariableStub.returns(null);
});

add_task(async function test_experiment_with_separate_default_notifies() {
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    false
  );
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );

  clearTelemetry();

  getVariableStub.callsFake(name =>
    name == "seperatePrivateDefaultUIEnabled" ? true : null
  );
  NimbusFeatures.searchConfiguration.onUpdate.firstCall.args[0]();

  await checkTelemetry("experiment", null, testChromeIconEngine, true);

  clearTelemetry();

  // Reset the stub so that we are no longer in an experiment.
  getVariableStub.returns(null);
  NimbusFeatures.searchConfiguration.onUpdate.firstCall.args[0]();

  await checkTelemetry("experiment", testChromeIconEngine, null, true);
});

add_task(async function test_default_engine_update() {
  clearTelemetry();
  let extension = await SearchTestUtils.installSearchExtension(
    {
      name: "engine",
      id: "engine@tests.mozilla.org",
      search_url_get_params: `q={searchTerms}&version=1.0`,
      search_url: "https://www.google.com/search",
      version: "1.0",
    },
    { skipUnload: true }
  );
  let engine = Services.search.getEngineByName("engine");

  Assert.ok(!!engine, "Should have loaded the engine");

  await Services.search.setDefault(
    engine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  clearTelemetry();

  let promiseChanged = TestUtils.topicObserved(
    "browser-search-engine-modified",
    (eng, verb) => verb == "engine-changed"
  );
  let manifest = SearchTestUtils.createEngineManifest({
    name: "Bar",
    id: "engine@tests.mozilla.org",
    search_url_get_params: `q={searchTerms}&version=2.0`,
    search_url: "https://www.google.com/search",
    version: "2.0",
  });

  await extension.upgrade({
    useAddonManager: "permanent",
    manifest,
  });
  await AddonTestUtils.waitForSearchProviderStartup(extension);
  await promiseChanged;

  const defaultEngineData = {
    id: engine.telemetryId,
    name: "Bar",
    loadPath: engine.wrappedJSObject._loadPath,
    submissionURL: "https://www.google.com/search?q=&version=2.0",
  };
  await checkTelemetry("engine-update", defaultEngineData, defaultEngineData);
  await extension.unload();
});
