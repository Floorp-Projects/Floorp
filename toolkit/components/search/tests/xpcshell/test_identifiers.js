/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that a search engine's identifier can be extracted from the filename.
 */

"use strict";

const SEARCH_APP_DIR = 1;

function run_test() {
  removeMetadata();
  removeCacheFile();
  do_load_manifest("data/chrome.manifest");

  let url  = "chrome://testsearchplugin/locale/searchplugins/";
  Services.prefs.setCharPref("browser.search.jarURIs", url);
  Services.prefs.setBoolPref("browser.search.loadFromJars", true);

  updateAppInfo();

  run_next_test();
}

add_test(function test_identifier() {
  let engineFile = gProfD.clone();
  engineFile.append("searchplugins");
  engineFile.append("test-search-engine.xml");
  engineFile.parent.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  // Copy the test engine to the test profile.
  let engineTemplateFile = do_get_file("data/engine.xml");
  engineTemplateFile.copyTo(engineFile.parent, "test-search-engine.xml");

  let search = Services.search.init(function initComplete(aResult) {
    do_print("init'd search service");
    do_check_true(Components.isSuccessCode(aResult));

    let profileEngine = Services.search.getEngineByName("Test search engine");
    let jarEngine = Services.search.getEngineByName("bug645970");

    do_check_true(profileEngine instanceof Ci.nsISearchEngine);
    do_check_true(jarEngine instanceof Ci.nsISearchEngine);

    // An engine loaded from the profile directory won't have an identifier,
    // because it's not built-in.
    do_check_eq(profileEngine.identifier, null);

    // An engine loaded from a JAR will have an identifier corresponding to
    // the filename inside the JAR. (In this case it's the same as the name.)
    do_check_eq(jarEngine.identifier, "bug645970");

    removeMetadata();
    removeCacheFile();
    run_next_test();
  });
});

