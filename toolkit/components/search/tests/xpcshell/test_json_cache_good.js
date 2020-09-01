/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test initializing from a broken search cache. This is one where the engines
 * array for some reason has lost all the default engines, but retained either
 * one or two, or a user-supplied engine. We don't know why this happens, but
 * we have seen it (bug 1578807).
 */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { getAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
);

var cacheTemplate, appPluginsPath, profPlugins;

const enginesCache = {
  version: SearchUtils.CACHE_VERSION,
  buildID: "TBD",
  appVersion: "TBD",
  locale: "en-US",
  visibleDefaultEngines: ["engine1", "engine2"],
  builtInEngineList: [
    { id: "engine1@search.mozilla.org", locale: "default" },
    { id: "engine2@search.mozilla.org", locale: "default" },
  ],
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

  await useTestEngines("data1");
  Services.prefs.setCharPref(SearchUtils.BROWSER_SEARCH_PREF + "region", "US");
  Services.locale.availableLocales = ["en-US"];
  Services.locale.requestedLocales = ["en-US"];

  // We dynamically generate the hashes because these depend on the profile.
  enginesCache.metaData.searchDefaultHash = SearchUtils.getVerificationHash(
    enginesCache.metaData.searchDefault
  );
  enginesCache.metaData.hash = SearchUtils.getVerificationHash(
    enginesCache.metaData.current
  );
  enginesCache.metaData.visibleDefaultEnginesHash = SearchUtils.getVerificationHash(
    enginesCache.metaData.visibleDefaultEngines
  );
  const appInfo = getAppInfo();
  enginesCache.buildID = appInfo.platformBuildID;
  enginesCache.appVersion = appInfo.version;

  await OS.File.writeAtomic(
    OS.Path.join(OS.Constants.Path.profileDir, CACHE_FILENAME),
    new TextEncoder().encode(JSON.stringify(enginesCache)),
    { compression: "lz4" }
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

  const scalars = Services.telemetry.getSnapshotForScalars("main", false)
    .parent;
  Assert.equal(
    scalars["browser.searchinit.engines_cache_corrupted"],
    false,
    "Should have recorded the engines cache as not corrupted"
  );

  const engines = await Services.search.getEngines();

  Assert.deepEqual(
    engines.map(e => e.name),
    ["engine1", "engine2"],
    "Should have the expected default engines"
  );
});
