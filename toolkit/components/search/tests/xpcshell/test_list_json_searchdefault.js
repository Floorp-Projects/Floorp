/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Check default search engine is picked from list.json searchDefault */

"use strict";

function run_test() {
  Assert.ok(!Services.search.isInitialized, "search isn't initialized yet");

  run_next_test();
}

// Check that current engine matches with US searchDefault from list.json
add_task(async function test_searchDefaultEngineUS() {
  Services.prefs.setCharPref("browser.search.region", "US");

  await asyncInit();

  Assert.ok(Services.search.isInitialized, "search initialized");

  Assert.equal(Services.search.currentEngine.name,
               getDefaultEngineName(true), "expected US default search engine");

  Services.prefs.clearUserPref("browser.search.region");
});
