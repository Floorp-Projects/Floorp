/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that currentEngine and defaultEngine properties are updated when the
 * prefs are set independently.
 */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://testing-common/httpd.js");

let waitForEngines = {
  "Test search engine": 1,
  "A second test engine": 1
};

const PREF_BRANCH = "browser.search.";

/**
 * Wrapper for nsIPrefBranch::setComplexValue.
 * @param aPrefName
 *        The name of the pref to set.
 */
function setLocalizedPref(aPrefName, aValue) {
  let nsIPLS = Ci.nsIPrefLocalizedString;
  let branch = Services.prefs.getBranch(PREF_BRANCH);
  try {
    var pls = Cc["@mozilla.org/pref-localizedstring;1"].
              createInstance(Ci.nsIPrefLocalizedString);
    pls.data = aValue;
    branch.setComplexValue(aPrefName, nsIPLS, pls);
  } catch (ex) {}
}

function search_observer(aSubject, aTopic, aData) {
  let engine = aSubject.QueryInterface(Ci.nsISearchEngine);
  do_print("Observer: " + aData + " for " + engine.name);

  if (aData != "engine-added") {
    return;
  }

  // If the engine is defined in `waitForEngines`, remove it from the list
  if (waitForEngines[engine.name]) {
    delete waitForEngines[engine.name];
  } else {
    // This engine is not one we're waiting for, so bail out early.
    return;
  }

  // Only continue when both engines have been loaded.
  if (Object.keys(waitForEngines).length) {
    return;
  }

  let search = Services.search;

  let engine1Name = "Test search engine";
  let engine2Name = "A second test engine";
  let engine1 = search.getEngineByName(engine1Name);
  let engine2 = search.getEngineByName(engine2Name);

  // Initial sanity check:
  search.defaultEngine = engine1;
  do_check_eq(search.defaultEngine, engine1);
  search.currentEngine = engine1;
  do_check_eq(search.currentEngine, engine1);

  setLocalizedPref("defaultenginename", engine2Name);
  // Default engine should be synced with the pref
  do_check_eq(search.defaultEngine, engine2);
  // Current engine should've stayed the same
  do_check_eq(search.currentEngine, engine1);
  
  setLocalizedPref("selectedEngine", engine2Name);
  // Default engine should've stayed the same
  do_check_eq(search.defaultEngine, engine2);
  // Current engine should've been updated
  do_check_eq(search.currentEngine, engine2);

  // Test that setting the currentEngine to the original default engine clears
  // the selectedEngine pref, rather than setting it. To do this we need to
  // set the value of defaultenginename on the default branch.
  let defaultBranch = Services.prefs.getDefaultBranch("");
  let prefName = PREF_BRANCH + "defaultenginename";
  let prefVal = "data:text/plain," + prefName + "=" + engine1Name;
  defaultBranch.setCharPref(prefName, prefVal, true);
  search.currentEngine = engine1;
  // Current engine should've been updated
  do_check_eq(search.currentEngine, engine1);
  do_check_false(Services.prefs.prefHasUserValue("browser.search.selectedEngine"));

  // Test that setting the defaultEngine to the original default engine clears
  // the defaultenginename pref, rather than setting it.
  do_check_true(Services.prefs.prefHasUserValue("browser.search.defaultenginename"));
  search.defaultEngine = engine1;
  do_check_eq(search.defaultEngine, engine1);
  do_check_false(Services.prefs.prefHasUserValue("browser.search.defaultenginename"));

  do_test_finished();
}

function run_test() {
  removeMetadata();
  updateAppInfo();

  let httpServer = new HttpServer();
  httpServer.start(-1);
  httpServer.registerDirectory("/", do_get_cwd());
  let baseUrl = "http://localhost:" + httpServer.identity.primaryPort;

  do_register_cleanup(function cleanup() {
    httpServer.stop(function() {});
    Services.obs.removeObserver(search_observer, "browser-search-engine-modified");
  });

  do_test_pending();

  Services.obs.addObserver(search_observer, "browser-search-engine-modified", false);

  Services.search.addEngine(baseUrl + "/data/engine.xml",
                   Ci.nsISearchEngine.DATA_XML,
                   null, false);
  Services.search.addEngine(baseUrl + "/data/engine2.xml",
                   Ci.nsISearchEngine.DATA_XML,
                   null, false);
}
