/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * test_nocache: Start search engine
 * - without search.json
 *
 * Ensure that :
 * - nothing explodes;
 * - search.json is created.
 */

function run_test()
{
  removeCache();
  updateAppInfo();
  do_load_manifest("data/chrome.manifest");
  useHttpServer();

  run_next_test();
}

add_task(function* test_nocache() {
  let search = Services.search;

  // Check that cache is created at startup
  afterCache(function cacheCreated() {
    // Check that search.json has been created.
    let cache = gProfD.clone();
    cache.append("search.json");
    do_check_true(cache.exists());
  });
  yield new Promise((resolve, reject) => search.init(rv => {
    Components.isSuccessCode(rv) ? resolve() : reject();
  }));

  // Add engine and wait for cache update
  yield addTestEngines([
    { name: "Test search engine", xmlFileName: "engine.xml" },
  ]);

  do_print("Engine has been added, let's wait for the cache to be built");
  yield new Promise(resolve => afterCache(resolve));

  do_print("Searching test engine in cache");
  let path = OS.Path.join(OS.Constants.Path.profileDir, "search.json");
  let data = yield OS.File.read(path);
  let text = new TextDecoder().decode(data);
  let cache = JSON.parse(text);
  let found = false;
  for (let dirName in cache.directories) {
    for (let engine of cache.directories[dirName].engines) {
      if (engine._id == "[app]/test-search-engine.xml") {
        found = true;
        break;
      }
    }
    if (found) {
      break;
    }
  }
  do_check_true(found);

  removeCache();
});
