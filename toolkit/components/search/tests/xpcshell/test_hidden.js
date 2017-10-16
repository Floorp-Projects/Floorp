/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const kUrlPref = "geoSpecificDefaults.url";

function run_test() {
  removeMetadata();
  removeCacheFile();

  do_load_manifest("data/chrome.manifest");

  configureToLoadJarEngines();

  // Geo specific defaults won't be fetched if there's no country code.
  Services.prefs.setCharPref("browser.search.geoip.url",
                             'data:application/json,{"country_code": "US"}');

  // Make 'hidden' the only visible engine.
 let url = "data:application/json,{\"interval\": 31536000, \"settings\": {\"searchDefault\": \"hidden\", \"visibleDefaultEngines\": [\"hidden\"]}}";
  Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF).setCharPref(kUrlPref, url);

  do_check_false(Services.search.isInitialized);

  run_next_test();
}

add_task(async function async_init() {
  let commitPromise = promiseAfterCache();
  await asyncInit();

  let engines = Services.search.getEngines();
  do_check_eq(engines.length, 1);

  // The default test jar engine has been hidden.
  let engine = Services.search.getEngineByName("bug645970");
  do_check_eq(engine, null);

  // The hidden engine is visible.
  engine = Services.search.getEngineByName("hidden");
  do_check_neq(engine, null);

  // The next test does a sync init, which won't do the geoSpecificDefaults XHR,
  // so it depends on the metadata having been written to disk.
  await commitPromise;
});

add_task(async function sync_init() {
  let unInitPromise = waitForSearchNotification("uninit-complete");
  let reInitPromise = asyncReInit();
  await unInitPromise;
  do_check_false(Services.search.isInitialized);

  // Synchronously check the current default engine, to force a sync init.
  do_check_eq(Services.search.currentEngine.name, "hidden");
  do_check_true(Services.search.isInitialized);

  let engines = Services.search.getEngines();
  do_check_eq(engines.length, 1);

  // The default test jar engine has been hidden.
  let engine = Services.search.getEngineByName("bug645970");
  do_check_eq(engine, null);

  // The hidden engine is visible.
  engine = Services.search.getEngineByName("hidden");
  do_check_neq(engine, null);

  await reInitPromise;
});

add_task(async function invalid_engine() {
  // Trigger a new request.
  await forceExpiration();

  // Set the visibleDefaultEngines list to something that contains a non-existent engine.
  // This should cause the search service to ignore the list altogether and fallback to
  // local defaults.
  let url = "data:application/json,{\"interval\": 31536000, \"settings\": {\"searchDefault\": \"hidden\", \"visibleDefaultEngines\": [\"hidden\", \"bogus\"]}}";
  Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF).setCharPref(kUrlPref, url);

  await asyncReInit();

  let engines = Services.search.getEngines();
  do_check_eq(engines.length, 1);

  // The default test jar engine is visible.
  let engine = Services.search.getEngineByName("bug645970");
  do_check_neq(engine, null);

  // The hidden engine is... hidden.
  engine = Services.search.getEngineByName("hidden");
  do_check_eq(engine, null);
});
