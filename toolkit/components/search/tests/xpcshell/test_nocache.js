/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * test_nocache: Start search engine
 * - without search.json.mozlz4
 *
 * Ensure that :
 * - nothing explodes;
 * - search.json.mozlz4 is created.
 */

function run_test() {
  do_load_manifest("data/chrome.manifest");
  useHttpServer();

  run_next_test();
}

add_task(async function test_nocache() {
  let search = Services.search;

  let afterCachePromise = promiseAfterCache();

  await new Promise((resolve, reject) => search.init(rv => {
    Components.isSuccessCode(rv) ? resolve() : reject();
  }));

  // Check that the cache is created at startup
  await afterCachePromise;

  // Check that search.json.mozlz4 has been created.
  let cacheFile = gProfD.clone();
  cacheFile.append(CACHE_FILENAME);
  Assert.ok(cacheFile.exists());

  // Add engine and wait for cache update
  await addTestEngines([
    { name: "Test search engine", xmlFileName: "engine.xml" },
  ]);

  info("Engine has been added, let's wait for the cache to be built");
  await promiseAfterCache();

  info("Searching test engine in cache");
  let cache = await promiseCacheData();
  let found = false;
  for (let engine of cache.engines) {
    if (engine._shortName == "test-search-engine") {
      found = true;
      break;
    }
  }
  Assert.ok(found);
});
