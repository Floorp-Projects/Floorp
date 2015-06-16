/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  removeMetadata();
  removeCacheFile();

  do_load_manifest("data/chrome.manifest");

  configureToLoadJarEngines();
  installAddonEngine();

  do_check_false(Services.search.isInitialized);

  // test the add-on engine is loaded in addition to our jar engine
  let engines = Services.search.getEngines();
  do_check_eq(engines.length, 2);

  do_check_true(Services.search.isInitialized);

  // test jar engine is loaded ok.
  let engine = Services.search.getEngineByName("addon");
  do_check_neq(engine, null);

  do_check_eq(engine.description, "addon");
}
