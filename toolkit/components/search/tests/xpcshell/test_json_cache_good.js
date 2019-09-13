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
const { getVerificationHash } = ChromeUtils.import(
  "resource://gre/modules/SearchEngine.jsm"
);

var cacheTemplate, appPluginsPath, profPlugins;

const enginesCache = {
  version: 1,
  buildID: "TBD",
  appVersion: "TBD",
  locale: "en-US",
  visibleDefaultEngines: [
    "engine",
    "engine-pref",
    "engine-rel-searchform-purpose",
    "engine-chromeicon",
    "engine-resourceicon",
    "engine-reordered",
  ],
  metaData: {
    searchDefault: "Test search engine",
    searchDefaultHash: "TBD",
    // Intentionally in the past, but shouldn't actually matter for this test.
    searchDefaultExpir: 1567694909002,
    current: "",
    hash: "TBD",
    visibleDefaultEngines:
      "engine,engine-pref,engine-rel-searchform-purpose,engine-chromeicon,engine-resourceicon,engine-reordered",
    visibleDefaultEnginesHash: "TBD",
  },
  engines: [
    {
      _readOnly: true,
      _urls: [
        {
          type: "text/html",
          method: "GET",
          params: [{ name: "q", value: "{searchTerms}", purpose: void 0 }],
          rels: [],
          mozparams: {},
          template: "https://1.example.com/search",
          templateHost: "1.example.com",
          resultDomain: "1.example.com",
        },
      ],
      _metaData: { alias: null },
      _shortName: "engine1",
      _extensionID: "engine1@search.mozilla.org",
      _isBuiltin: true,
      _queryCharset: "UTF-8",
      _name: "engine1",
      _description: "A small test engine",
      __searchForm: null,
      _iconURI: {},
      _hasPreferredIcon: true,
      _iconMapObj: {
        "{}":
          "moz-extension://090ca958-5ebb-f24e-a33c-e027d682491b/favicon.ico",
      },
      _loadPath: "[other]addEngineWithDetails:engine1@search.mozilla.org",
    },
    {
      _readOnly: true,
      _urls: [
        {
          type: "text/html",
          method: "GET",
          params: [{ name: "q", value: "{searchTerms}", purpose: void 0 }],
          rels: [],
          mozparams: {},
          template: "https://2.example.com/search",
          templateHost: "2.example.com",
          resultDomain: "2.example.com",
        },
      ],
      _metaData: { alias: null },
      _shortName: "engine2",
      _extensionID: "engine2@search.mozilla.org",
      _isBuiltin: true,
      _queryCharset: "UTF-8",
      _name: "engine2",
      _description: "A small test engine",
      __searchForm: null,
      _iconURI: {},
      _hasPreferredIcon: true,
      _iconMapObj: {
        "{}":
          "moz-extension://ec9d0671-9f8f-a24d-99b1-2a6590c0aa51/favicon.ico",
      },
      _loadPath: "[other]addEngineWithDetails:engine2@search.mozilla.org",
    },
  ],
};

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();

  useTestEngineConfig("resource://test/data1/");
  Services.prefs.setCharPref(SearchUtils.BROWSER_SEARCH_PREF + "region", "US");
  Services.locale.availableLocales = ["en-US"];
  Services.locale.requestedLocales = ["en-US"];

  // We dynamically generate the hashes because these depend on the profile.
  enginesCache.metaData.searchDefaultHash = getVerificationHash(
    enginesCache.metaData.searchDefault
  );
  enginesCache.metaData.hash = getVerificationHash(
    enginesCache.metaData.current
  );
  enginesCache.metaData.visibleDefaultEnginesHash = getVerificationHash(
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
