/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  removeMetadata();
  removeCacheFile();

  do_load_manifest("data/chrome.manifest");

  configureToLoadJarEngines();
  installDistributionEngine();

  do_check_false(Services.search.isInitialized);

  // test that the engine from the distribution overrides our jar engine
  let engines = Services.search.getEngines();
  do_check_eq(engines.length, 1);

  do_check_true(Services.search.isInitialized);

  let engine = Services.search.getEngineByName("bug645970");
  do_check_neq(engine, null);

  // check the engine we have is actually the one from the distribution
  do_check_eq(engine.description, "override");
}
