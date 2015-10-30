/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const NS_APP_USER_SEARCH_DIR  = "UsrSrchPlugns";

function run_test() {
  do_test_pending();

  // Copy an engine to [profile]/searchplugin/
  let dir = Services.dirsvc.get(NS_APP_USER_SEARCH_DIR, Ci.nsIFile);
  if (!dir.exists())
    dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  do_get_file("data/engine-override.xml").copyTo(dir, "bug645970.xml");

  let file = dir.clone();
  file.append("bug645970.xml");
  do_check_true(file.exists());

  do_check_false(Services.search.isInitialized);

  Services.search.init(function search_initialized(aStatus) {
    do_check_true(Components.isSuccessCode(aStatus));
    do_check_true(Services.search.isInitialized);

    // test the engine is loaded ok.
    let engine = Services.search.getEngineByName("bug645970");
    do_check_neq(engine, null);

    // remove the engine and verify the file has been removed too.
    Services.search.removeEngine(engine);
    do_check_false(file.exists());

    do_test_finished();
  });
}
