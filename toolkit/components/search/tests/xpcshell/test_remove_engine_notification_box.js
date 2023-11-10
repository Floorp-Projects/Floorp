/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CONFIG = [
  {
    // Engine initially default, but the defaults will be changed to engine-pref.
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
    appliesTo: [
      {
        included: { everywhere: true },
        default: "yes",
      },
      {
        included: { regions: ["FR"] },
        default: "no",
      },
    ],
  },
  {
    // This will become defaults when region is changed to FR.
    webExtension: {
      id: "engine-pref@search.mozilla.org",
      name: "engine-pref",
      search_url: "https://www.google.com/search",
      params: [
        {
          name: "q",
          value: "{searchTerms}",
        },
      ],
    },
    appliesTo: [
      {
        included: { everywhere: true },
      },
      {
        included: { regions: ["FR"] },
        default: "yes",
      },
    ],
  },
];

const CONFIG_UPDATED = CONFIG.filter(r =>
  r.webExtension.id.startsWith("engine-pref")
);

let stub;
let settingsFilePath;
let userSettings;

add_setup(async function () {
  SearchSettings.SETTINGS_INVALIDATION_DELAY = 100;
  SearchTestUtils.useMockIdleService();
  await SearchTestUtils.useTestEngines("data", null, CONFIG);
  await AddonTestUtils.promiseStartupManager();

  stub = sinon.stub(
    await Services.search.wrappedJSObject,
    "_showRemovalOfSearchEngineNotificationBox"
  );

  settingsFilePath = PathUtils.join(PathUtils.profileDir, SETTINGS_FILENAME);

  Region._setHomeRegion("", false);

  let promiseSaved = promiseAfterSettings();
  await Services.search.init();
  await promiseSaved;

  userSettings = await Services.search.wrappedJSObject._settings.get();
});

// Verify the loaded configuration matches what we expect for the test.
add_task(async function test_initial_config_correct() {
  const installedEngines = await Services.search.getAppProvidedEngines();
  Assert.deepEqual(
    installedEngines.map(e => e.identifier),
    ["engine", "engine-pref"],
    "Should have the correct list of engines installed."
  );

  Assert.equal(
    (await Services.search.getDefault()).identifier,
    "engine",
    "Should have loaded the expected default engine"
  );
});

add_task(async function test_metadata_undefined() {
  let defaultEngineChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.DEFAULT,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );

  info("Update region to FR.");
  Region._setHomeRegion("FR", false);

  let settings = structuredClone(userSettings);
  settings.metaData = undefined;
  await reloadEngines(settings);
  Assert.ok(
    stub.notCalled,
    "_reloadEngines should not have shown the notification box."
  );

  settings = structuredClone(userSettings);
  settings.metaData = undefined;
  await loadEngines(settings);
  Assert.ok(
    stub.notCalled,
    "_loadEngines should not have shown the notification box."
  );

  const newDefault = await defaultEngineChanged;
  Assert.equal(
    newDefault.QueryInterface(Ci.nsISearchEngine).name,
    "engine-pref",
    "Should have correctly notified the new default engine."
  );
});

add_task(async function test_metadata_changed() {
  let metaDataProperties = [
    "locale",
    "region",
    "channel",
    "experiment",
    "distroID",
  ];

  for (let name of metaDataProperties) {
    let settings = structuredClone(userSettings);
    settings.metaData[name] = "test";
    await assert_metadata_changed(settings);
  }
});

add_task(async function test_default_engine_unchanged() {
  let currentEngineName =
    Services.search.wrappedJSObject._getEngineDefault(false).name;

  Assert.equal(
    currentEngineName,
    "Test search engine",
    "Default engine should be unchanged."
  );

  await reloadEngines(structuredClone(userSettings));
  Assert.ok(
    stub.notCalled,
    "_reloadEngines should not have shown the notification box."
  );

  await loadEngines(structuredClone(userSettings));
  Assert.ok(
    stub.notCalled,
    "_loadEngines should not have shown the notification box."
  );
});

add_task(async function test_new_current_engine_is_undefined() {
  consoleAllowList.push("No default engine");
  let settings = structuredClone(userSettings);
  let getEngineDefaultStub = sinon.stub(
    await Services.search.wrappedJSObject,
    "_getEngineDefault"
  );
  getEngineDefaultStub.returns(undefined);

  await loadEngines(settings);
  Assert.ok(
    stub.notCalled,
    "_loadEngines should not have shown the notification box."
  );

  getEngineDefaultStub.restore();
});

add_task(async function test_current_engine_is_null() {
  Services.search.wrappedJSObject._currentEngine = null;

  await reloadEngines(structuredClone(userSettings));
  Assert.ok(
    stub.notCalled,
    "_reloadEngines should not have shown the notification box."
  );

  let settings = structuredClone(userSettings);
  settings.metaData.current = null;
  await loadEngines(settings);
  Assert.ok(
    stub.notCalled,
    "_loadEngines should not have shown the notification box."
  );
});

add_task(async function test_default_changed_and_metadata_unchanged_exists() {
  info("Update region to FR to change engine.");
  Region._setHomeRegion("FR", false);

  info("Set user settings metadata to the same properties as cached metadata.");
  await Services.search.wrappedJSObject._fetchEngineSelectorEngines();
  userSettings.metaData = {
    ...Services.search.wrappedJSObject._settings.getSettingsMetaData(),
    appDefaultEngine: "Test search engine",
  };

  await reloadEngines(structuredClone(userSettings));
  Assert.ok(
    stub.notCalled,
    "_reloadEngines should not show the notification box as the engine still exists."
  );

  // Reset.
  Region._setHomeRegion("US", false);
  await reloadEngines(structuredClone(userSettings));
});

add_task(async function test_default_engine_changed_and_metadata_unchanged() {
  info("Update region to FR to change engine.");
  Region._setHomeRegion("FR", false);

  const defaultEngineChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.DEFAULT,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );

  info("Set user settings metadata to the same properties as cached metadata.");
  await Services.search.wrappedJSObject._fetchEngineSelectorEngines();
  userSettings.metaData = {
    ...Services.search.wrappedJSObject._settings.getSettingsMetaData(),
    appDefaultEngineId: "engine@search.mozilla.orgdefault",
  };

  // Update config by removing the app default engine
  await setConfigToLoad(CONFIG_UPDATED);

  await reloadEngines(structuredClone(userSettings));
  Assert.ok(
    stub.calledOnce,
    "_reloadEngines should show the notification box."
  );

  Assert.deepEqual(
    stub.firstCall.args,
    ["Test search engine", "engine-pref"],
    "_showRemovalOfSearchEngineNotificationBox should display " +
      "'Test search engine' as the engine removed and 'engine-pref' as the new " +
      "default engine."
  );

  const newDefault = await defaultEngineChanged;
  Assert.equal(
    newDefault.QueryInterface(Ci.nsISearchEngine).name,
    "engine-pref",
    "Should have correctly notified the new default engine"
  );

  info("Reset userSettings.metaData.current engine.");
  let settings = structuredClone(userSettings);
  settings.metaData.current = Services.search.wrappedJSObject._currentEngine;

  await loadEngines(settings);
  Assert.ok(stub.calledTwice, "_loadEngines should show the notification box.");

  Assert.deepEqual(
    stub.secondCall.args,
    ["Test search engine", "engine-pref"],
    "_showRemovalOfSearchEngineNotificationBox should display " +
      "'Test search engine' as the engine removed and 'engine-pref' as the new " +
      "default engine."
  );
});

add_task(async function test_app_default_engine_changed_on_start_up() {
  let settings = structuredClone(userSettings);

  // Set the current engine to "" so we can use the app default engine as
  // default
  settings.metaData.current = "";

  // Update config by removing the app default engine
  await setConfigToLoad(CONFIG_UPDATED);

  await loadEngines(settings);
  Assert.ok(
    stub.calledThrice,
    "_loadEngines should show the notification box."
  );
});

add_task(async function test_app_default_engine_change_start_up_still_exists() {
  stub.resetHistory();
  let settings = structuredClone(userSettings);

  // Set the current engine to "" so we can use the app default engine as
  // default
  settings.metaData.current = "";
  settings.metaData.appDefaultEngine = "Test search engine";

  await setConfigToLoad(CONFIG);

  await loadEngines(settings);
  Assert.ok(
    stub.notCalled,
    "_loadEngines should not show the notification box."
  );
});

async function setConfigToLoad(config) {
  let searchSettingsObj = await RemoteSettings(SearchUtils.SETTINGS_KEY);
  // Restore the get method in order to stub it again in useTestEngines
  searchSettingsObj.get.restore();
  Services.search.wrappedJSObject.resetEngineSelector();
  await SearchTestUtils.useTestEngines("data", null, config);
}

function writeSettings(settings) {
  return IOUtils.writeJSON(settingsFilePath, settings, { compress: true });
}

async function reloadEngines(settings) {
  let promiseSaved = promiseAfterSettings();

  await Services.search.wrappedJSObject._reloadEngines(settings);

  await promiseSaved;
}

async function loadEngines(settings) {
  await writeSettings(settings);

  let promiseSaved = promiseAfterSettings();

  Services.search.wrappedJSObject.reset();
  await Services.search.init();

  await promiseSaved;
}

async function assert_metadata_changed(settings) {
  info("Update region.");
  Region._setHomeRegion("FR", false);
  await reloadEngines(settings);
  Region._setHomeRegion("", false);

  let defaultEngineChanged = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.DEFAULT,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );

  await reloadEngines(settings);
  Assert.ok(
    stub.notCalled,
    "_reloadEngines should not have shown the notification box."
  );

  let newDefault = await defaultEngineChanged;
  Assert.equal(
    newDefault.QueryInterface(Ci.nsISearchEngine).name,
    "Test search engine",
    "Should have correctly notified the new default engine."
  );

  Region._setHomeRegion("FR", false);
  await reloadEngines(settings);
  Region._setHomeRegion("", false);

  await loadEngines(settings);
  Assert.ok(
    stub.notCalled,
    "_loadEngines should not have shown the notification box."
  );

  Assert.equal(
    Services.search.defaultEngine.name,
    "Test search engine",
    "Should have correctly notified the new default engine."
  );
}
