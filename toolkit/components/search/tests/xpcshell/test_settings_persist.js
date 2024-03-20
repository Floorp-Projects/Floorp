/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests the removal of an engine is persisted in search settings.
 */

"use strict";

const CONFIG_UPDATED = [
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
    },
    appliesTo: [{ included: { everywhere: true } }],
  },
];

const SEARCH_CONFIG_V2_UPDATED = [
  {
    recordType: "engine",
    identifier: "plainengine",
    base: {
      name: "Plain",
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
    globalDefault: "plainengine",
    specificDefaults: [],
  },
  {
    recordType: "engineOrders",
    orders: [],
  },
];

async function startup() {
  let settingsFileWritten = promiseAfterSettings();
  let ss = new SearchService();
  await AddonTestUtils.promiseRestartManager();
  await ss.init(false);
  await settingsFileWritten;
  return ss;
}

async function updateConfig(config) {
  const settings = await RemoteSettings(SearchUtils.SETTINGS_KEY);
  settings.get.restore();

  config == "test-extensions"
    ? await SearchTestUtils.useTestEngines("test-extensions")
    : sinon.stub(settings, "get").returns(config);
}

async function visibleEngines(ss) {
  return (await ss.getVisibleEngines()).map(e => e._name);
}

add_setup(async function () {
  await SearchTestUtils.useTestEngines("test-extensions");
  registerCleanupFunction(AddonTestUtils.promiseShutdownManager);
  await AddonTestUtils.promiseStartupManager();
  // This is only needed as otherwise events will not be properly notified
  // due to https://searchfox.org/mozilla-central/rev/5f0a7ca8968ac5cef8846e1d970ef178b8b76dcc/toolkit/components/search/SearchSettings.sys.mjs#41-42
  let settingsFileWritten = promiseAfterSettings();
  await Services.search.init(false);
  Services.search.wrappedJSObject._removeObservers();
  await settingsFileWritten;
});

add_task(async function () {
  let ss = await startup();
  Assert.ok(
    (await visibleEngines(ss)).includes("Special"),
    "Should have both engines on first startup"
  );

  let settingsFileWritten = promiseAfterSettings();
  let engine = await ss.getEngineByName("Special");
  await ss.removeEngine(engine);
  await settingsFileWritten;

  Assert.ok(
    !(await visibleEngines(ss)).includes("Special"),
    "Special has been remove, only Plain should remain"
  );

  ss._removeObservers();
  await updateConfig(
    SearchUtils.newSearchConfigEnabled
      ? SEARCH_CONFIG_V2_UPDATED
      : CONFIG_UPDATED
  );
  ss = await startup();

  Assert.ok(
    !(await visibleEngines(ss)).includes("Special"),
    "Updated to new configuration that doesnt have Special"
  );

  ss._removeObservers();
  await updateConfig("test-extensions");

  ss = await startup();

  Assert.ok(
    !(await visibleEngines(ss)).includes("Special"),
    "Configuration now includes Special but we should remember its removal"
  );
});
