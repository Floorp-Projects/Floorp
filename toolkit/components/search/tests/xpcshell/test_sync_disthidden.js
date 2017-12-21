/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Test that the ignoredJAREngines pref hides search engines  */

"use strict";

function run_test() {
  let defaultBranch = Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF);
  defaultBranch.setCharPref("ignoredJAREngines", "engine");
  Services.prefs.setCharPref("distribution.id", "partner-1");

  // The test engines used in this test need to be recognized as 'default'
  // engines or the resource URL won't be used
  let url = "resource://test/data/";
  let resProt = Services.io.getProtocolHandler("resource")
                        .QueryInterface(Ci.nsIResProtocolHandler);
  resProt.setSubstitution("search-plugins", Services.io.newURI(url));

  Assert.ok(!Services.search.isInitialized);

  let engines = Services.search.getEngines();
  // From data/list.json - only 5 out of 6
  // since one is hidden
  Assert.equal(engines.length, 5);

  Assert.ok(Services.search.isInitialized);

  // Verify that the Test search engine is not available
  let engine = Services.search.getEngineByName("Test search engine");
  Assert.equal(engine, null);
}
