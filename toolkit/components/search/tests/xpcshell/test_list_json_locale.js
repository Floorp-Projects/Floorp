/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Check default search engine is picked from list.json searchDefault */

"use strict";

function run_test() {
  Assert.ok(!Services.search.isInitialized, "search isn't initialized yet");

  run_next_test();
}

// Override list.json with test data from data/list.json
// and check that different locale is working
add_task(async function test_listJSONlocale() {
  let url = "resource://test/data/";
  let resProt = Services.io.getProtocolHandler("resource")
                        .QueryInterface(Ci.nsIResProtocolHandler);
  resProt.setSubstitution("search-plugins", Services.io.newURI(url));

  Services.locale.setAvailableLocales(["de"]);
  Services.locale.setRequestedLocales(["de"]);

  await asyncInit();

  Assert.ok(Services.search.isInitialized, "search initialized");

  let sortedEngines = Services.search.getEngines();
  Assert.equal(sortedEngines.length, 1, "Should have only one engine");
});


// Check that switching locale switches search engines
add_task(async function test_listJSONlocaleSwitch() {
  let promise = waitForSearchNotification("reinit-complete");

  Services.locale.setAvailableLocales(["fr"]);
  Services.locale.setRequestedLocales(["fr"]);

  await promise;

  Assert.ok(Services.search.isInitialized, "search initialized");

  let sortedEngines = Services.search.getEngines();
  Assert.equal(sortedEngines.length, 2, "Should have two engines");
});

// Check that region overrides apply
add_task(async function test_listJSONRegionOverride() {
  Services.prefs.setCharPref("browser.search.region", "RU");

  await asyncReInit();

  Assert.ok(Services.search.isInitialized, "search initialized");

  let sortedEngines = Services.search.getEngines();
  Assert.equal(sortedEngines.length, 2, "Should have two engines");
  Assert.equal(sortedEngines[0].identifier, "engine-chromeicon", "Engine should have been overridden by engine-chromeicon");
});
