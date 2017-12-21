/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  do_test_pending();

  do_load_manifest("data/chrome.manifest");

  configureToLoadJarEngines();

  Assert.ok(!Services.search.isInitialized);
  let fallback = false;

  Services.search.init(function search_initialized(aStatus) {
    Assert.ok(fallback);
    Assert.ok(Components.isSuccessCode(aStatus));
    Assert.ok(Services.search.isInitialized);

    // test engines from dir are not loaded.
    let engines = Services.search.getEngines();
    Assert.equal(engines.length, 1);

    // test jar engine is loaded ok.
    let engine = Services.search.getEngineByName("bug645970");
    Assert.notEqual(engine, null);

    do_test_finished();
  });

  // Execute test for the sync fallback while the async code is being executed.
  Services.obs.addObserver(function searchServiceObserver(aResult, aTopic, aVerb) {
    if (aVerb == "find-jar-engines") {
      Services.obs.removeObserver(searchServiceObserver, aTopic);
      fallback = true;

      Assert.ok(!Services.search.isInitialized);

      // test engines from dir are not loaded.
      let engines = Services.search.getEngines();
      Assert.equal(engines.length, 1);

      // test jar engine is loaded ok.
      let engine = Services.search.getEngineByName("bug645970");
      Assert.notEqual(engine, null);

      Assert.ok(Services.search.isInitialized);
    }
  }, "browser-search-service");
}
