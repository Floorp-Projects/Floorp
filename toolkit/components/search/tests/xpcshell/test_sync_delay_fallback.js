/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  do_test_pending();

  removeMetadata();
  removeCacheFile();

  do_load_manifest("data/chrome.manifest");

  let url  = "chrome://testsearchplugin/locale/searchplugins/";
  Services.prefs.setCharPref("browser.search.jarURIs", url);
  Services.prefs.setBoolPref("browser.search.loadFromJars", true);

  do_check_false(Services.search.isInitialized);
  let fallback = false;

  Services.search.init(function search_initialized(aStatus) {
    do_check_true(fallback);
    do_check_true(Components.isSuccessCode(aStatus));
    do_check_true(Services.search.isInitialized);

    // test engines from dir are loaded.
    let engines = Services.search.getEngines();
    do_check_true(engines.length > 1);

    // test jar engine is loaded ok.
    let engine = Services.search.getEngineByName("bug645970");
    do_check_neq(engine, null);

    Services.prefs.clearUserPref("browser.search.jarURIs");
    Services.prefs.clearUserPref("browser.search.loadFromJars");

    do_test_finished();
  });

  // Execute test for the sync fallback while the async code is being executed.
  Services.obs.addObserver(function searchServiceObserver(aResult, aTopic, aVerb) {
    if (aVerb == "find-jar-engines") {
      Services.obs.removeObserver(searchServiceObserver, aTopic);
      fallback = true;

      do_check_false(Services.search.isInitialized);

      // test engines from dir are loaded.
      let engines = Services.search.getEngines();
      do_check_true(engines.length > 1);

      // test jar engine is loaded ok.
      let engine = Services.search.getEngineByName("bug645970");
      do_check_neq(engine, null);

      do_check_true(Services.search.isInitialized);
    }
  }, "browser-search-service", false);
}
