/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  do_test_pending();

  removeMetadata();
  removeCacheFile();

  do_load_manifest("data/chrome.manifest");

  configureToLoadJarEngines(false);

  do_check_false(Services.search.isInitialized);

  Services.search.init(function search_initialized(aStatus) {
    do_check_true(Components.isSuccessCode(aStatus));
    do_check_true(Services.search.isInitialized);

    // test engine from dir is loaded.
    let engine = Services.search.getEngineByName("TestEngineApp");
    do_check_neq(engine, null);

    // test jar engine is not loaded.
    engine = Services.search.getEngineByName("bug645970");
    do_check_eq(engine, null);

    do_test_finished();
  });
}
