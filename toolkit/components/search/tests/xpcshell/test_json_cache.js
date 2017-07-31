/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test initializing from the search cache.
 */

"use strict";

/**
 * Gets a directory from the directory service.
 * @param aKey
 *        The directory service key indicating the directory to get.
 */
var _dirSvc = null;
function getDir(aKey, aIFace) {
  if (!aKey) {
    do_throw("getDir requires a directory key!");
  }

  if (!_dirSvc) {
    _dirSvc = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIProperties);
  }
  return _dirSvc.get(aKey, aIFace || Ci.nsIFile);
}

function makeURI(uri) {
  return Services.io.newURI(uri);
}

var cacheTemplate, appPluginsPath, profPlugins;

/**
 * Test reading from search.json.mozlz4
 */
function run_test() {
  removeMetadata();
  removeCacheFile();

  let cacheTemplateFile = do_get_file("data/search.json");
  cacheTemplate = readJSONFile(cacheTemplateFile);
  cacheTemplate.buildID = getAppInfo().platformBuildID;

  let engineFile = gProfD.clone();
  engineFile.append("searchplugins");
  engineFile.append("test-search-engine.xml");
  engineFile.parent.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  // Copy the test engine to the test profile.
  let engineTemplateFile = do_get_file("data/engine.xml");
  engineTemplateFile.copyTo(engineFile.parent, "test-search-engine.xml");

  // The list of visibleDefaultEngines needs to match or the cache will be ignored.
  let chan = NetUtil.newChannel({
    uri: "resource://search-plugins/list.json",
    loadUsingSystemPrincipal: true
  });
  let sis = Cc["@mozilla.org/scriptableinputstream;1"].
              createInstance(Ci.nsIScriptableInputStream);
  sis.init(chan.open2());
  let list = sis.read(sis.available());
  let searchSettings = JSON.parse(list);

  cacheTemplate.visibleDefaultEngines = searchSettings["default"]["visibleDefaultEngines"];

  run_next_test();
}

add_test(function prepare_test_data() {
  OS.File.writeAtomic(OS.Path.join(OS.Constants.Path.profileDir, CACHE_FILENAME),
                      new TextEncoder().encode(JSON.stringify(cacheTemplate)),
                      {compression: "lz4"})
    .then(run_next_test);
});

/**
 * Start the search service and confirm the engine properties match the expected values.
 */
add_test(function test_cached_engine_properties() {
  do_print("init search service");

  Services.search.init(function initComplete(aResult) {
    do_print("init'd search service");
    do_check_true(Components.isSuccessCode(aResult));

    let engines = Services.search.getEngines({});
    let engine = engines[0];

    do_check_true(engine instanceof Ci.nsISearchEngine);
    isSubObjectOf(EXPECTED_ENGINE.engine, engine);

    let engineFromSS = Services.search.getEngineByName(EXPECTED_ENGINE.engine.name);
    do_check_true(!!engineFromSS);
    isSubObjectOf(EXPECTED_ENGINE.engine, engineFromSS);

    removeMetadata();
    removeCacheFile();
    run_next_test();
  });
});

/**
 * Test that the JSON cache written in the profile is correct.
 */
add_test(function test_cache_write() {
  do_print("test cache writing");

  let cache = gProfD.clone();
  cache.append(CACHE_FILENAME);
  do_check_false(cache.exists());

  do_print("Next step is forcing flush");
  do_timeout(0, function forceFlush() {
    do_print("Forcing flush");
    // Force flush
    // Note: the timeout is needed, to avoid some reentrency
    // issues in nsSearchService.

    let cacheWriteObserver = {
      observe: function cacheWriteObserver_observe(aEngine, aTopic, aVerb) {
        if (aTopic != "browser-search-service" || aVerb != "write-cache-to-disk-complete") {
          return;
        }
        Services.obs.removeObserver(cacheWriteObserver, "browser-search-service");
        do_print("Cache write complete");
        do_check_true(cache.exists());
        // Check that the search.json.mozlz4 cache matches the template

        promiseCacheData().then(cacheWritten => {
          do_print("Check search.json.mozlz4");
          isSubObjectOf(cacheTemplate, cacheWritten);

          run_next_test();
        });
      }
    };
    Services.obs.addObserver(cacheWriteObserver, "browser-search-service");

    Services.search.QueryInterface(Ci.nsIObserver).observe(null, "browser-search-engine-modified", "engine-removed");
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
      "_iconURL": "data:image/png;base64,AAABAAEAEBAAAAEAGABoAwAAFgAAACgAAAAQAAAAIAAAAAEAGAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADs9Pt8xetPtu9FsfFNtu%2BTzvb2%2B%2Fne4dFJeBw0egA%2FfAJAfAA8ewBBegAAAAD%2B%2FPtft98Mp%2BwWsfAVsvEbs%2FQeqvF8xO7%2F%2F%2F63yqkxdgM7gwE%2FggM%2BfQA%2BegBDeQDe7PIbotgQufcMufEPtfIPsvAbs%2FQvq%2Bfz%2Bf%2F%2B%2B%2FZKhR05hgBBhQI8hgBAgAI9ewD0%2B%2Fg3pswAtO8Cxf4Kw%2FsJvvYAqupKsNv%2B%2Fv7%2F%2FP5VkSU0iQA7jQA9hgBDgQU%2BfQH%2F%2Ff%2FQ6fM4sM4KsN8AteMCruIqqdbZ7PH8%2Fv%2Fg6Nc%2Fhg05kAA8jAM9iQI%2BhQA%2BgQDQu6b97uv%2F%2F%2F7V8Pqw3eiWz97q8%2Ff%2F%2F%2F%2F7%2FPptpkkqjQE4kwA7kAA5iwI8iAA8hQCOSSKdXjiyflbAkG7u2s%2F%2B%2F%2F39%2F%2F7r8utrqEYtjQE8lgA7kwA7kwA9jwA9igA9hACiWSekVRyeSgiYSBHx6N%2F%2B%2Fv7k7OFRmiYtlAA5lwI7lwI4lAA7kgI9jwE9iwI4iQCoVhWcTxCmb0K%2BooT8%2Fv%2F7%2F%2F%2FJ2r8fdwI1mwA3mQA3mgA8lAE8lAE4jwA9iwE%2BhwGfXifWvqz%2B%2Ff%2F58u%2Fev6Dt4tr%2B%2F%2F2ZuIUsggA7mgM6mAM3lgA5lgA6kQE%2FkwBChwHt4dv%2F%2F%2F728ei1bCi7VAC5XQ7kz7n%2F%2F%2F6bsZkgcB03lQA9lgM7kwA2iQktZToPK4r9%2F%2F%2F9%2F%2F%2FSqYK5UwDKZAS9WALIkFn%2B%2F%2F3%2F%2BP8oKccGGcIRJrERILYFEMwAAuEAAdX%2F%2Ff7%2F%2FP%2B%2BfDvGXQLIZgLEWgLOjlf7%2F%2F%2F%2F%2F%2F9QU90EAPQAAf8DAP0AAfMAAOUDAtr%2F%2F%2F%2F7%2B%2Fu2bCTIYwDPZgDBWQDSr4P%2F%2Fv%2F%2F%2FP5GRuABAPkAA%2FwBAfkDAPAAAesAAN%2F%2F%2B%2Fz%2F%2F%2F64g1C5VwDMYwK8Yg7y5tz8%2Fv%2FV1PYKDOcAAP0DAf4AAf0AAfYEAOwAAuAAAAD%2F%2FPvi28ymXyChTATRrIb8%2F%2F3v8fk6P8MAAdUCAvoAAP0CAP0AAfYAAO4AAACAAQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAACAAQAA",
      _urls: [
        {
          type: "application/x-suggestions+json",
          method: "GET",
          template: "http://suggestqueries.google.com/complete/search?output=firefox&client=firefox" +
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
              "name": "q",
              "value": "{searchTerms}",
              "purpose": undefined,
            },
            {
              "name": "ie",
              "value": "utf-8",
              "purpose": undefined,
            },
            {
              "name": "oe",
              "value": "utf-8",
              "purpose": undefined,
            },
            {
              "name": "aq",
              "value": "t",
              "purpose": undefined,
            },
            {
              "name": "channel",
              "value": "fflb",
              "purpose": "keyword",
            },
            {
              "name": "channel",
              "value": "rcs",
              "purpose": "contextmenu",
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
              "name": "q",
              "value": "{searchTerms}",
              "purpose": undefined,
            },
            {
              "name": "channel",
              "value": "fflb",
              "purpose": "keyword",
            },
            {
              "name": "channel",
              "value": "rcs",
              "purpose": "contextmenu",
            },
          ],
        },
      ],
    },
  },
};
