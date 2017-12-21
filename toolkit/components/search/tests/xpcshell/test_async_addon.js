/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  do_test_pending();

  do_load_manifest("data/chrome.manifest");

  configureToLoadJarEngines();
  installAddonEngine();

  Assert.ok(!Services.search.isInitialized);

  Services.search.init(function search_initialized(aStatus) {
    Assert.ok(Components.isSuccessCode(aStatus));
    Assert.ok(Services.search.isInitialized);

    // test the legacy add-on engine is _not_ loaded
    let engines = Services.search.getEngines();
    Assert.equal(engines.length, 1);
    Assert.equal(Services.search.getEngineByName("addon"), null);

    do_test_finished();
  });
}
