/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Check default search engine is picked from list.json searchDefault */

"use strict";

// Override list.json with test data from data/list.json
// and check that different locale is working
add_task(async function test_listJSONlocale() {
  let url = "resource://test/data/";
  let resProt = Services.io.getProtocolHandler("resource")
                        .QueryInterface(Ci.nsIResProtocolHandler);
  resProt.setSubstitution("search-extensions", Services.io.newURI(url));

  Services.locale.availableLocales = ["de"];
  Services.locale.requestedLocales = ["de"];

  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();

  Assert.ok(Services.search.isInitialized, "search initialized");

  let sortedEngines = await Services.search.getEngines();
  Assert.equal(sortedEngines.length, 1, "Should have only one engine");
});


// Check that switching locale switches search engines
add_task(async function test_listJSONlocaleSwitch() {
  let promise = SearchTestUtils.promiseSearchNotification("reinit-complete");

  let defaultBranch = Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF);
  defaultBranch.setCharPref("param.code", "good&id=unique");

  Services.locale.availableLocales = ["fr"];
  Services.locale.requestedLocales = ["fr"];

  await promise;

  Assert.ok(Services.search.isInitialized, "search initialized");

  let sortedEngines = await Services.search.getEngines();
  Assert.equal(sortedEngines.length, 2, "Should have two engines");
  // Assert.equal(sortedEngines[0].identifier, "engine-pref");
});

// Check that region overrides apply
add_task(async function test_listJSONRegionOverride() {
  Services.prefs.setCharPref("browser.search.region", "RU");

  await asyncReInit({skipReset: true});

  Assert.ok(Services.search.isInitialized, "search initialized");

  let sortedEngines = await Services.search.getEngines();
  Assert.equal(sortedEngines.length, 2, "Should have two engines");
  Assert.equal(sortedEngines[0].identifier, "engine-chromeicon", "Engine should have been overridden by engine-chromeicon");
});
