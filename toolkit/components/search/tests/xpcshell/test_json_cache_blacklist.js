/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test initializing from the search cache.
 */

"use strict";

var cacheTemplate, appPluginsPath, profPlugins;

/**
 * Test reading from search.json.mozlz4
 */
function run_test() {
  let cacheTemplateFile = do_get_file("data/search_blacklist.json");
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
  cacheTemplate.visibleDefaultEngines = getDefaultEngineList(false);

  run_next_test();
}

add_test(function prepare_test_data() {
  promiseSaveCacheData(cacheTemplate).then(run_next_test);
});

/**
 * Start the search service and confirm the cache was reset
 */
add_test(function test_cache_rest() {
  info("init search service");

  Services.search.init(function initComplete(aResult) {
    info("init'd search service");
    Assert.ok(Components.isSuccessCode(aResult));

    let engines = Services.search.getEngines({});

    // Engine list will have been reset to the default,
    // Not the one engine in the cache.
    // It should have more than one engine.
    Assert.ok(engines.length > 1);

    removeCacheFile();
    run_next_test();
  });
});
