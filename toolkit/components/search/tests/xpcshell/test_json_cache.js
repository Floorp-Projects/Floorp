/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test initializing from the search cache.
 */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { getAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
);
const { SearchService } = ChromeUtils.import(
  "resource://gre/modules/SearchService.jsm"
);

var cacheTemplate, appPluginsPath, profPlugins;

/**
 * Test reading from search.json.mozlz4
 */
add_task(async function setup() {
  await useTestEngines("data1");
  await AddonTestUtils.promiseStartupManager();
});

async function loadCacheFile(cacheFile) {
  let cacheTemplateFile = do_get_file(cacheFile);
  cacheTemplate = readJSONFile(cacheTemplateFile);
  cacheTemplate.buildID = getAppInfo().platformBuildID;
  cacheTemplate.version = SearchUtils.CACHE_VERSION;

  if (gModernConfig) {
    delete cacheTemplate.visibleDefaultEngines;
  } else {
    // The list of visibleDefaultEngines needs to match or the cache will be ignored.
    cacheTemplate.visibleDefaultEngines = getDefaultEngineList(false);

    // Since the above code is querying directly from list.json,
    // we need to override the values in the esr case.
    if (AppConstants.MOZ_APP_VERSION_DISPLAY.endsWith("esr")) {
      let esrOverrides = {
        "google-b-d": "google-b-e",
        "google-b-1-d": "google-b-1-e",
      };

      for (let engine in esrOverrides) {
        let index = cacheTemplate.visibleDefaultEngines.indexOf(engine);
        if (index > -1) {
          cacheTemplate.visibleDefaultEngines[index] = esrOverrides[engine];
        }
      }
    }
  }

  await promiseSaveCacheData(cacheTemplate);

  if (!gModernConfig) {
    // For the legacy config, make sure we switch _isBuiltin to _isAppProvided
    // after saving, so that the expected results match.
    for (let engine of cacheTemplate.engines) {
      if ("_isBuiltin" in engine) {
        engine._isAppProvided = engine._isBuiltin;
        delete engine._isBuiltin;
      }
      if ("extensionID" in engine) {
        engine._extensionID = engine.extensionID;
        delete engine.extensionID;
      }
    }
  }
}

/**
 * Start the search service and confirm the engine properties match the expected values.
 *
 * @param {string} cacheFile
 *   The path to the cache file to use.
 */
async function checkLoadCachedProperties(cacheFile) {
  info("init search service");

  await loadCacheFile(cacheFile);

  const cacheFileWritten = promiseAfterCache();
  let ss = new SearchService();
  let result = await ss.init();

  info("init'd search service");
  Assert.ok(Components.isSuccessCode(result));

  await cacheFileWritten;

  let engines = await ss.getEngines();

  Assert.equal(
    engines[0].name,
    "engine1",
    "Should have loaded the correct first engine"
  );
  Assert.equal(engines[0].alias, "testAlias", "Should have set the alias");
  Assert.equal(engines[0].hidden, false, "Should have not hidden the engine");
  Assert.equal(
    engines[1].name,
    "engine2",
    "Should have loaded the correct second engine"
  );
  Assert.equal(engines[1].alias, null, "Should have not set the alias");
  Assert.equal(engines[1].hidden, true, "Should have hidden the engine");

  // The extra engine is the second in the list.
  isSubObjectOf(EXPECTED_ENGINE.engine, engines[2]);

  let engineFromSS = ss.getEngineByName(EXPECTED_ENGINE.engine.name);
  Assert.ok(!!engineFromSS);
  isSubObjectOf(EXPECTED_ENGINE.engine, engineFromSS);

  removeCacheFile();
  ss._removeObservers();
}

add_task(async function test_legacy_cached_engine_properties() {
  await checkLoadCachedProperties("data/search-legacy.json");
});

add_task(async function test_current_cached_engine_properties() {
  // Legacy configuration doesn't support loading the modern cache directly.
  if (!gModernConfig) {
    return;
  }
  await checkLoadCachedProperties("data/search.json");
});

/**
 * Test that the JSON cache written in the profile is correct.
 */
add_task(async function test_cache_write() {
  info("test cache writing");

  await loadCacheFile(
    gModernConfig ? "data/search.json" : "data/search-legacy.json"
  );

  const cacheFileWritten = promiseAfterCache();
  await Services.search.init();
  await cacheFileWritten;
  removeCacheFile();

  let cache = do_get_profile().clone();
  cache.append(CACHE_FILENAME);
  Assert.ok(!cache.exists());

  info("Next step is forcing flush");
  // Note: the dispatch is needed, to avoid some reentrency
  // issues in SearchService.
  let cacheWritePromise = promiseAfterCache();

  Services.tm.dispatchToMainThread(() => {
    // Call the observe method directly to simulate a remove but not actually
    // remove anything.
    Services.search.wrappedJSObject._cache
      .QueryInterface(Ci.nsIObserver)
      .observe(null, "browser-search-engine-modified", "engine-removed");
  });

  await cacheWritePromise;

  info("Cache write complete");
  Assert.ok(cache.exists());
  // Check that the search.json.mozlz4 cache matches the template

  let cacheData = await promiseCacheData();
  info("Check search.json.mozlz4");
  isSubObjectOf(cacheTemplate, cacheData, (prop, value) => {
    // Skip items that are to do with icons for extensions, as we can't
    // control the uuid.
    if (prop != "_iconURL" && prop != "{}") {
      return false;
    }
    return value.startsWith("moz-extension://");
  });
});

var EXPECTED_ENGINE = {
  engine: {
    name: "Test search engine",
    alias: null,
    description: "A test search engine (based on Google search)",
    searchForm: "http://www.google.com/",
    wrappedJSObject: {
      _extensionID: "test-addon-id@mozilla.org",
      _iconURL:
        "data:image/png;base64,AAABAAEAEBAAAAEAGABoAwAAFgAAACgAAAAQAAA" +
        "AIAAAAAEAGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADs9Pt8xetPtu9F" +
        "sfFNtu%2BTzvb2%2B%2Fne4dFJeBw0egA%2FfAJAfAA8ewBBegAAAAD%2B%2F" +
        "Ptft98Mp%2BwWsfAVsvEbs%2FQeqvF8xO7%2F%2F%2F63yqkxdgM7gwE%2Fgg" +
        "M%2BfQA%2BegBDeQDe7PIbotgQufcMufEPtfIPsvAbs%2FQvq%2Bfz%2Bf%2F" +
        "%2B%2B%2FZKhR05hgBBhQI8hgBAgAI9ewD0%2B%2Fg3pswAtO8Cxf4Kw%2FsJ" +
        "vvYAqupKsNv%2B%2Fv7%2F%2FP5VkSU0iQA7jQA9hgBDgQU%2BfQH%2F%2Ff%" +
        "2FQ6fM4sM4KsN8AteMCruIqqdbZ7PH8%2Fv%2Fg6Nc%2Fhg05kAA8jAM9iQI%" +
        "2BhQA%2BgQDQu6b97uv%2F%2F%2F7V8Pqw3eiWz97q8%2Ff%2F%2F%2F%2F7%" +
        "2FPptpkkqjQE4kwA7kAA5iwI8iAA8hQCOSSKdXjiyflbAkG7u2s%2F%2B%2F%" +
        "2F39%2F%2F7r8utrqEYtjQE8lgA7kwA7kwA9jwA9igA9hACiWSekVRyeSgiYS" +
        "BHx6N%2F%2B%2Fv7k7OFRmiYtlAA5lwI7lwI4lAA7kgI9jwE9iwI4iQCoVhWc" +
        "TxCmb0K%2BooT8%2Fv%2F7%2F%2F%2FJ2r8fdwI1mwA3mQA3mgA8lAE8lAE4j" +
        "wA9iwE%2BhwGfXifWvqz%2B%2Ff%2F58u%2Fev6Dt4tr%2B%2F%2F2ZuIUsgg" +
        "A7mgM6mAM3lgA5lgA6kQE%2FkwBChwHt4dv%2F%2F%2F728ei1bCi7VAC5XQ7" +
        "kz7n%2F%2F%2F6bsZkgcB03lQA9lgM7kwA2iQktZToPK4r9%2F%2F%2F9%2F%" +
        "2F%2FSqYK5UwDKZAS9WALIkFn%2B%2F%2F3%2F%2BP8oKccGGcIRJrERILYFE" +
        "MwAAuEAAdX%2F%2Ff7%2F%2FP%2B%2BfDvGXQLIZgLEWgLOjlf7%2F%2F%2F%" +
        "2F%2F%2F9QU90EAPQAAf8DAP0AAfMAAOUDAtr%2F%2F%2F%2F7%2B%2Fu2bCT" +
        "IYwDPZgDBWQDSr4P%2F%2Fv%2F%2F%2FP5GRuABAPkAA%2FwBAfkDAPAAAesA" +
        "AN%2F%2F%2B%2Fz%2F%2F%2F64g1C5VwDMYwK8Yg7y5tz8%2Fv%2FV1PYKDOc" +
        "AAP0DAf4AAf0AAfYEAOwAAuAAAAD%2F%2FPvi28ymXyChTATRrIb8%2F%2F3v" +
        "8fk6P8MAAdUCAvoAAP0CAP0AAfYAAO4AAACAAQAAAAAAAAAAAAAAAAAAAAAAA" +
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACAAQAA",
      _urls: [
        {
          type: "application/x-suggestions+json",
          method: "GET",
          template:
            "http://suggestqueries.google.com/complete/search?output=firefox&client=firefox" +
            "&hl={moz:locale}&q={searchTerms}",
          params: "",
        },
        {
          type: "text/html",
          method: "GET",
          template: "http://www.google.com/search",
          resultDomain: "google.com",
          params: [
            {
              name: "q",
              value: "{searchTerms}",
              purpose: undefined,
            },
            {
              name: "channel",
              value: "fflb",
              purpose: "keyword",
            },
            {
              name: "channel",
              value: "rcs",
              purpose: "contextmenu",
            },
          ],
        },
        {
          type: "application/x-moz-default-purpose",
          method: "GET",
          template: "http://www.google.com/search",
          resultDomain: "purpose.google.com",
          params: [
            {
              name: "q",
              value: "{searchTerms}",
              purpose: undefined,
            },
            {
              name: "channel",
              value: "fflb",
              purpose: "keyword",
            },
            {
              name: "channel",
              value: "rcs",
              purpose: "contextmenu",
            },
          ],
        },
      ],
    },
  },
};
