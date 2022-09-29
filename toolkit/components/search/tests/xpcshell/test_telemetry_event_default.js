/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests for the default engine telemetry event that can be tested via xpcshell,
 * related to changing or selecting a different configuration.
 * Other tests are typically in browser mochitests.
 */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.jsm",
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.jsm",
});

const BASE_CONFIG = [
  {
    webExtension: { id: "engine@search.mozilla.org" },
    appliesTo: [{ included: { everywhere: true } }],
    default: "yes",
  },
];
const MAIN_CONFIG = [
  {
    webExtension: { id: "engine@search.mozilla.org" },
    appliesTo: [{ included: { everywhere: true } }],
    default: "no",
  },
  {
    webExtension: { id: "engine-chromeicon@search.mozilla.org" },
    appliesTo: [{ included: { everywhere: true } }],
    default: "yes-if-no-other",
  },
  {
    webExtension: { id: "engine-fr@search.mozilla.org" },
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
    webExtension: { id: "engine-pref@search.mozilla.org" },
    appliesTo: [
      { included: { everywhere: true } },
      { included: { regions: ["DE"] }, default: "yes" },
    ],
    default: "no",
  },
  {
    webExtension: { id: "engine2@search.mozilla.org" },
    appliesTo: [
      { included: { everywhere: true } },
      { included: { everywhere: true }, experiment: "test1", default: "yes" },
    ],
    default: "no",
  },
];

const testSearchEngine = {
  id: "engine",
  name: "Test search engine",
  loadPath: "[other]addEngineWithDetails:engine@search.mozilla.org",
  submissionUrl: "https://www.google.com/search?q=",
};
const testChromeIconEngine = {
  id: "engine-chromeicon",
  name: "engine-chromeicon",
  loadPath: "[other]addEngineWithDetails:engine-chromeicon@search.mozilla.org",
  submissionUrl: "https://www.google.com/search?q=",
};
const testFrEngine = {
  id: "engine-fr",
  name: "Test search engine (fr)",
  loadPath: "[other]addEngineWithDetails:engine-fr@search.mozilla.org",
  submissionUrl: "https://www.google.fr/search?q=&ie=iso-8859-1&oe=iso-8859-1",
};
const testPrefEngine = {
  id: "engine-pref",
  name: "engine-pref",
  loadPath: "[other]addEngineWithDetails:engine-pref@search.mozilla.org",
  submissionUrl: "https://www.google.com/search?q=",
};
const testEngine2 = {
  id: "engine2",
  name: "A second test engine",
  loadPath: "[other]addEngineWithDetails:engine2@search.mozilla.org",
  submissionUrl: "https://duckduckgo.com/?q=",
};

function clearTelemetry() {
  Services.telemetry.clearEvents();
  Services.fog.testResetFOG();
}

async function checkTelemetry(
  source,
  prevEngine,
  newEngine,
  checkPrivate = false
) {
  TelemetryTestUtils.assertEvents(
    [
      {
        object: checkPrivate ? "change_private" : "change_default",
        value: source,
        extra: {
          prev_id: prevEngine?.id ?? "",
          new_id: newEngine?.id ?? "",
          new_name: newEngine?.name ?? "",
          new_load_path: newEngine?.loadPath ?? "",
          // Telemetry has a limit of 80 characters.
          new_sub_url: newEngine?.submissionUrl.slice(0, 80) ?? "",
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
        new_submission_url: newEngine?.submissionUrl ?? "",
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

  sinon.spy(NimbusFeatures.search, "onUpdate");
  sinon.stub(NimbusFeatures.search, "ready").resolves();
  getVariableStub = sinon.stub(NimbusFeatures.search, "getVariable");
  getVariableStub.returns(null);

  SearchTestUtils.useMockIdleService();
  Services.fog.initializeFOG();
  sinon.stub(
    Services.search.wrappedJSObject,
    "_showRemovalOfSearchEngineNotificationBox"
  );

  await SearchTestUtils.useTestEngines("data", null, BASE_CONFIG);
  await AddonTestUtils.promiseStartupManager();

  await Services.search.init();
});

add_task(async function test_configuration_changes_default() {
  clearTelemetry();

  await SearchTestUtils.updateRemoteSettingsConfig(MAIN_CONFIG);

  await checkTelemetry("config", testSearchEngine, testChromeIconEngine);
});

add_task(async function test_experiment_changes_default() {
  clearTelemetry();

  let reloadObserved = SearchTestUtils.promiseSearchNotification(
    "engines-reloaded"
  );
  getVariableStub.callsFake(name => (name == "experiment" ? "test1" : null));
  NimbusFeatures.search.onUpdate.firstCall.args[0]();
  await reloadObserved;

  await checkTelemetry("experiment", testChromeIconEngine, testEngine2);

  // Reset the stub so that we are no longer in an experiment.
  getVariableStub.returns(null);
});

add_task(async function test_locale_changes_default() {
  clearTelemetry();

  let reloadObserved = SearchTestUtils.promiseSearchNotification(
    "engines-reloaded"
  );
  Services.locale.requestedLocales = ["fr"];
  await reloadObserved;

  await checkTelemetry("locale", testEngine2, testFrEngine);
});

add_task(async function test_region_changes_default() {
  clearTelemetry();

  let reloadObserved = SearchTestUtils.promiseSearchNotification(
    "engines-reloaded"
  );
  Region._setHomeRegion("DE", true);
  await reloadObserved;

  await checkTelemetry("region", testFrEngine, testPrefEngine);
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
  NimbusFeatures.search.onUpdate.firstCall.args[0]();

  await checkTelemetry("experiment", null, testChromeIconEngine, true);

  clearTelemetry();

  // Reset the stub so that we are no longer in an experiment.
  getVariableStub.returns(null);
  NimbusFeatures.search.onUpdate.firstCall.args[0]();

  await checkTelemetry("experiment", testChromeIconEngine, null, true);
});
