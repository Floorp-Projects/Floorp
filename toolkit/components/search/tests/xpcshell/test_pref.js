/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Test that MozParam condition="pref" values used in search URLs are from the
 * default branch, and that their special characters are URL encoded. */

"use strict";

function run_test() {
  updateAppInfo();

  // The test engines used in this test need to be recognized as 'default'
  // engines, or their MozParams will be ignored.
  let url = "resource://test/data/";
  let resProt = Services.io.getProtocolHandler("resource")
                        .QueryInterface(Ci.nsIResProtocolHandler);
  resProt.setSubstitution("search-plugins",
                          Services.io.newURI(url, null, null));

  run_next_test();
}

add_task(function* test_pref() {
  let defaultBranch = Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF);
  defaultBranch.setCharPref("param.code", "good&id=unique");
  Services.prefs.setCharPref(BROWSER_SEARCH_PREF + "param.code", "bad");

  yield asyncInit();

  let engine = Services.search.getEngineByName("engine-pref");
  let base = "http://www.google.com/search?q=foo&code=";
  do_check_eq(engine.getSubmission("foo").uri.spec,
              base + "good%26id%3Dunique");

  do_test_finished();
});
