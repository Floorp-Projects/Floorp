/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  do_load_manifest("data/chrome.manifest");

  configureToLoadJarEngines();
  installAddonEngine();

  do_check_false(Services.search.isInitialized);

  // test the legacy add-on engine is _not_ loaded
  let engines = Services.search.getEngines();
  do_check_true(Services.search.isInitialized);
  do_check_eq(engines.length, 1);
  do_check_eq(Services.search.getEngineByName("addon"), null);
}
