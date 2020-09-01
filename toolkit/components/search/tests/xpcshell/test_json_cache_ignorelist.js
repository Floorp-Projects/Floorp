/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test initializing from the search cache.
 */

"use strict";

var { getAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
);

var cacheTemplate, appPluginsPath, profPlugins;

/**
 * Test reading from search.json.mozlz4
 */
add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();

  await setupRemoteSettings();

  let cacheTemplateFile = do_get_file("data/search_ignorelist.json");
  cacheTemplate = readJSONFile(cacheTemplateFile);
  cacheTemplate.buildID = getAppInfo().platformBuildID;

  // The list of visibleDefaultEngines needs to match or the cache will be ignored.
  cacheTemplate.visibleDefaultEngines = getDefaultEngineList(false);

  await promiseSaveCacheData(cacheTemplate);
});

/**
 * Start the search service and confirm the cache was reset
 */
add_task(async function test_cache_rest() {
  info("init search service");

  let updatePromise = SearchTestUtils.promiseSearchNotification(
    "settings-update-complete"
  );

  let result = await Services.search.init();

  Assert.ok(
    Components.isSuccessCode(result),
    "Search service should be successfully initialized"
  );
  await updatePromise;

  const engines = await Services.search.getEngines();

  // Engine list will have been reset to the default,
  // Not the one engine in the cache.
  // It should have more than one engine.
  Assert.greater(
    engines.length,
    1,
    "Should have more than one engine in the list"
  );

  removeCacheFile();
});
