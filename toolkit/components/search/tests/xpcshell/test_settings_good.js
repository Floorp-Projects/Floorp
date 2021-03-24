/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test initializing from good search settings.
 */

"use strict";

const { getAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
);

const enginesSettings = {
  version: SearchUtils.SETTINGS_VERSION,
  buildID: "TBD",
  appVersion: "TBD",
  locale: "en-US",
  metaData: {
    searchDefault: "Test search engine",
    searchDefaultHash: "TBD",
    // Intentionally in the past, but shouldn't actually matter for this test.
    searchDefaultExpir: 1567694909002,
    current: "",
    hash: "TBD",
    visibleDefaultEngines: "engine1,engine2",
    visibleDefaultEnginesHash: "TBD",
  },
  engines: [
    {
      _metaData: { alias: null },
      _isAppProvided: true,
      _name: "engine1",
    },
    {
      _metaData: { alias: null },
      _isAppProvided: true,
      _name: "engine2",
    },
  ],
};

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();

  // Allow telemetry probes which may otherwise be disabled for some applications (e.g. Thunderbird)
  Services.prefs.setBoolPref(
    "toolkit.telemetry.testing.overrideProductsCheck",
    true
  );

  await SearchTestUtils.useTestEngines("data1");
  Services.prefs.setCharPref(SearchUtils.BROWSER_SEARCH_PREF + "region", "US");
  Services.locale.availableLocales = ["en-US"];
  Services.locale.requestedLocales = ["en-US"];

  // We dynamically generate the hashes because these depend on the profile.
  enginesSettings.metaData.searchDefaultHash = SearchUtils.getVerificationHash(
    enginesSettings.metaData.searchDefault
  );
  enginesSettings.metaData.hash = SearchUtils.getVerificationHash(
    enginesSettings.metaData.current
  );
  enginesSettings.metaData.visibleDefaultEnginesHash = SearchUtils.getVerificationHash(
    enginesSettings.metaData.visibleDefaultEngines
  );
  const appInfo = getAppInfo();
  enginesSettings.buildID = appInfo.platformBuildID;
  enginesSettings.appVersion = appInfo.version;

  await IOUtils.writeJSON(
    PathUtils.join(await PathUtils.getProfileDir(), SETTINGS_FILENAME),
    enginesSettings,
    { compress: true }
  );
});

add_task(async function test_cached_engine_properties() {
  info("init search service");

  const initResult = await Services.search.init();

  info("init'd search service");
  Assert.ok(
    Components.isSuccessCode(initResult),
    "Should have successfully created the search service"
  );

  const engines = await Services.search.getEngines();

  Assert.deepEqual(
    engines.map(e => e.name),
    ["engine1", "engine2"],
    "Should have the expected default engines"
  );
});
