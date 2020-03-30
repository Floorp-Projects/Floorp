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

// The cache here reflects what is currently saved by a distribution when it
// is overriding the defaults. The most significant bit here is that it is
// flagged as a non-built-in engine, this used to defeat the corruption
// handling, and would force reload of the cache each time.
// Bug 1623597 will correct these engines to be marked as built-in.
const enginesCache = gModernConfig
  ? {
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
          _name: "engine3",
          _shortName: "engine3",
          _loadPath: "[distribution]/searchplugins/common/engine3.xml",
          description: "A small test engine",
          __searchForm: "https://distro.example.com/",
          _iconURL:
            "data:image/png;base64,AAABAAEAEBAAAAEAGABoAwAAFgAAACgAAAAQAAAAIAAAAAEAGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADs9Pt8xetPtu9FsfFNtu%2BTzvb2%2B%2Fne4dFJeBw0egA%2FfAJAfAA8ewBBegAAAAD%2B%2FPtft98Mp%2BwWsfAVsvEbs%2FQeqvF8xO7%2F%2F%2F63yqkxdgM7gwE%2FggM%2BfQA%2BegBDeQDe7PIbotgQufcMufEPtfIPsvAbs%2FQvq%2Bfz%2Bf%2F%2B%2B%2FZKhR05hgBBhQI8hgBAgAI9ewD0%2B%2Fg3pswAtO8Cxf4Kw%2FsJvvYAqupKsNv%2B%2Fv7%2F%2FP5VkSU0iQA7jQA9hgBDgQU%2BfQH%2F%2Ff%2FQ6fM4sM4KsN8AteMCruIqqdbZ7PH8%2Fv%2Fg6Nc%2Fhg05kAA8jAM9iQI%2BhQA%2BgQDQu6b97uv%2F%2F%2F7V8Pqw3eiWz97q8%2Ff%2F%2F%2F%2F7%2FPptpkkqjQE4kwA7kAA5iwI8iAA8hQCOSSKdXjiyflbAkG7u2s%2F%2B%2F%2F39%2F%2F7r8utrqEYtjQE8lgA7kwA7kwA9jwA9igA9hACiWSekVRyeSgiYSBHx6N%2F%2B%2Fv7k7OFRmiYtlAA5lwI7lwI4lAA7kgI9jwE9iwI4iQCoVhWcTxCmb0K%2BooT8%2Fv%2F7%2F%2F%2FJ2r8fdwI1mwA3mQA3mgA8lAE8lAE4jwA9iwE%2BhwGfXifWvqz%2B%2Ff%2F58u%2Fev6Dt4tr%2B%2F%2F2ZuIUsggA7mgM6mAM3lgA5lgA6kQE%2FkwBChwHt4dv%2F%2F%2F728ei1bCi7VAC5XQ7kz7n%2F%2F%2F6bsZkgcB03lQA9lgM7kwA2iQktZToPK4r9%2F%2F%2F9%2F%2F%2FSqYK5UwDKZAS9WALIkFn%2B%2F%2F3%2F%2BP8oKccGGcIRJrERILYFEMwAAuEAAdX%2F%2Ff7%2F%2FP%2B%2BfDvGXQLIZgLEWgLOjlf7%2F%2F%2F%2F%2F%2F9QU90EAPQAAf8DAP0AAfMAAOUDAtr%2F%2F%2F%2F7%2B%2Fu2bCTIYwDPZgDBWQDSr4P%2F%2Fv%2F%2F%2FP5GRuABAPkAA%2FwBAfkDAPAAAesAAN%2F%2F%2B%2Fz%2F%2F%2F64g1C5VwDMYwK8Yg7y5tz8%2Fv%2FV1PYKDOcAAP0DAf4AAf0AAfYEAOwAAuAAAAD%2F%2FPvi28ymXyChTATRrIb8%2F%2F3v8fk6P8MAAdUCAvoAAP0CAP0AAfYAAO4AAACAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACAAQAA",
          _metaData: { alias: null },
          _urls: [
            {
              params: [{ name: "q", value: "{searchTerms}" }],
              rels: [],
              resultDomain: "distro.example.com",
              template: "https://distro.example.com/search",
            },
          ],
          _isBuiltin: true,
          _orderHint: null,
          _telemetryId: null,
          queryCharset: "UTF-8",
        },
        {
          _metaData: { alias: null },
          _isBuiltin: true,
          _name: "engine1",
        },
        {
          _metaData: { alias: null },
          _isBuiltin: true,
          _name: "engine2",
        },
      ],
    }
  : {
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
          extensionID: "engine1@search.mozilla.org",
          extensionLocale: "default",
          _isBuiltin: true,
          _queryCharset: "UTF-8",
          _name: "engine1",
          _description: "A small test engine",
          __searchForm: null,
          _iconURI: {},
          _hasPreferredIcon: true,
          _iconMapObj: {
            "{}":
              "moz-extension://ec9d0671-9f8f-a24d-99b1-2a6590c0aa51/favicon.ico",
          },
          _loadPath: "[other]addEngineWithDetails:engine1@search.mozilla.org",
        },
        {
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
          extensionID: "engine2@search.mozilla.org",
          extensionLocale: "default",
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
        {
          _name: "engine3",
          _shortName: "engine3",
          _loadPath: "[distribution]/searchplugins/common/engine3.xml",
          description: "A small test engine3",
          __searchForm: "https://distro.example.com/",
          _iconURL:
            "data:image/png;base64,AAABAAEAEBAAAAEAGABoAwAAFgAAACgAAAAQAAAAIAAAAAEAGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADs9Pt8xetPtu9FsfFNtu%2BTzvb2%2B%2Fne4dFJeBw0egA%2FfAJAfAA8ewBBegAAAAD%2B%2FPtft98Mp%2BwWsfAVsvEbs%2FQeqvF8xO7%2F%2F%2F63yqkxdgM7gwE%2FggM%2BfQA%2BegBDeQDe7PIbotgQufcMufEPtfIPsvAbs%2FQvq%2Bfz%2Bf%2F%2B%2B%2FZKhR05hgBBhQI8hgBAgAI9ewD0%2B%2Fg3pswAtO8Cxf4Kw%2FsJvvYAqupKsNv%2B%2Fv7%2F%2FP5VkSU0iQA7jQA9hgBDgQU%2BfQH%2F%2Ff%2FQ6fM4sM4KsN8AteMCruIqqdbZ7PH8%2Fv%2Fg6Nc%2Fhg05kAA8jAM9iQI%2BhQA%2BgQDQu6b97uv%2F%2F%2F7V8Pqw3eiWz97q8%2Ff%2F%2F%2F%2F7%2FPptpkkqjQE4kwA7kAA5iwI8iAA8hQCOSSKdXjiyflbAkG7u2s%2F%2B%2F%2F39%2F%2F7r8utrqEYtjQE8lgA7kwA7kwA9jwA9igA9hACiWSekVRyeSgiYSBHx6N%2F%2B%2Fv7k7OFRmiYtlAA5lwI7lwI4lAA7kgI9jwE9iwI4iQCoVhWcTxCmb0K%2BooT8%2Fv%2F7%2F%2F%2FJ2r8fdwI1mwA3mQA3mgA8lAE8lAE4jwA9iwE%2BhwGfXifWvqz%2B%2Ff%2F58u%2Fev6Dt4tr%2B%2F%2F2ZuIUsggA7mgM6mAM3lgA5lgA6kQE%2FkwBChwHt4dv%2F%2F%2F728ei1bCi7VAC5XQ7kz7n%2F%2F%2F6bsZkgcB03lQA9lgM7kwA2iQktZToPK4r9%2F%2F%2F9%2F%2F%2FSqYK5UwDKZAS9WALIkFn%2B%2F%2F3%2F%2BP8oKccGGcIRJrERILYFEMwAAuEAAdX%2F%2Ff7%2F%2FP%2B%2BfDvGXQLIZgLEWgLOjlf7%2F%2F%2F%2F%2F%2F9QU90EAPQAAf8DAP0AAfMAAOUDAtr%2F%2F%2F%2F7%2B%2Fu2bCTIYwDPZgDBWQDSr4P%2F%2Fv%2F%2F%2FP5GRuABAPkAA%2FwBAfkDAPAAAesAAN%2F%2F%2B%2Fz%2F%2F%2F64g1C5VwDMYwK8Yg7y5tz8%2Fv%2FV1PYKDOcAAP0DAf4AAf0AAfYEAOwAAuAAAAD%2F%2FPvi28ymXyChTATRrIb8%2F%2F3v8fk6P8MAAdUCAvoAAP0CAP0AAfYAAO4AAACAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACAAQAA",
          _metaData: { alias: null },
          _urls: [
            {
              params: [{ name: "q", value: "{searchTerms}" }],
              rels: [],
              resultDomain: "distro.example.com",
              template: "https://distro.example.com/search",
            },
          ],
          _isBuiltin: true,
          _orderHint: null,
          _telemetryId: null,
          queryCharset: "UTF-8",
        },
      ],
    };

add_task(async function setup() {
  Services.prefs
    .getDefaultBranch(null)
    .setCharPref("distribution.id", "partner-test");
  installDistributionEngine("data1/engine3.xml", "engine3.xml");

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
    // Modern config forces the default engine to be first.
    ["engine1", "engine2", "engine3"],
    "Should have the expected default engines"
  );
});
