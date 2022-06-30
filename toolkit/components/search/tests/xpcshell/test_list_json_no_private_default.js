/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// TODO: Test fallback to normal default when no private set at all.

/* Check default search engine is picked from list.json searchDefault */

"use strict";

// Check that current engine matches with US searchDefault from list.json
add_task(async function test_searchDefaultEngineUS() {
  await SearchTestUtils.useTestEngines("data1");

  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );
  Services.prefs.setCharPref(SearchUtils.BROWSER_SEARCH_PREF + "region", "US");

  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();

  Assert.ok(Services.search.isInitialized, "search initialized");

  Assert.equal(
    Services.search.appDefaultEngine.name,
    "engine1",
    "Should have the expected engine as app default"
  );
  Assert.equal(
    Services.search.defaultEngine.name,
    "engine1",
    "Should have the expected engine as default"
  );
  Assert.equal(
    Services.search.appPrivateDefaultEngine.name,
    "engine1",
    "Should have the same engine for the app private default"
  );
  Assert.equal(
    Services.search.defaultPrivateEngine.name,
    "engine1",
    "Should have the same engine for the private default"
  );
});
