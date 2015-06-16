/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const NS_APP_USER_SEARCH_DIR  = "UsrSrchPlugns";

function run_test() {
  removeMetadata();
  removeCacheFile();

  do_load_manifest("data/chrome.manifest");

  configureToLoadJarEngines();

  // Copy an engine in [profile]/searchplugin/ and ensure it's not
  // overriding the same file from a jar.
  // The description in the file we are copying is 'profile'.
  let dir = Services.dirsvc.get(NS_APP_USER_SEARCH_DIR, Ci.nsIFile);
  if (!dir.exists())
    dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  do_get_file("data/engine-override.xml").copyTo(dir, "bug645970.xml");

  do_check_false(Services.search.isInitialized);

  // test engines from dir are not loaded.
  let engines = Services.search.getEngines();
  do_check_eq(engines.length, 1);

  do_check_true(Services.search.isInitialized);

  // test jar engine is loaded ok.
  let engine = Services.search.getEngineByName("bug645970");
  do_check_neq(engine, null);

  do_check_eq(engine.description, "bug645970");
}
