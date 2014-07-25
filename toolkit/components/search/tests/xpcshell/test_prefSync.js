/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that currentEngine and defaultEngine properties are updated when the
 * prefs are set independently.
 */

"use strict";

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

function run_test() {
  removeMetadata();
  updateAppInfo();
  useHttpServer();

  run_next_test();
}

add_task(function* test_prefSync() {
  let [engine1, engine2] = yield addTestEngines([
    { name: "Test search engine", xmlFileName: "engine.xml" },
    { name: "A second test engine", xmlFileName: "engine2.xml" },
  ]);

  let search = Services.search;

  // Initial sanity check:
  search.defaultEngine = engine1;
  do_check_eq(search.defaultEngine, engine1);
  search.currentEngine = engine1;
  do_check_eq(search.currentEngine, engine1);

  setLocalizedPref("defaultenginename", engine2.name);
  // Default engine should be synced with the pref
  do_check_eq(search.defaultEngine, engine2);
  // Current engine should've stayed the same
  do_check_eq(search.currentEngine, engine1);
  
  setLocalizedPref("selectedEngine", engine2.name);
  // Default engine should've stayed the same
  do_check_eq(search.defaultEngine, engine2);
  // Current engine should've been updated
  do_check_eq(search.currentEngine, engine2);

  // Test that setting the currentEngine to the original default engine clears
  // the selectedEngine pref, rather than setting it. To do this we need to
  // set the value of defaultenginename on the default branch.
  let defaultBranch = Services.prefs.getDefaultBranch("");
  let prefName = PREF_BRANCH + "defaultenginename";
  let prefVal = "data:text/plain," + prefName + "=" + engine1.name;
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
});
