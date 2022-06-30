/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Check default search engine is picked from list.json searchDefault */

"use strict";

// Check that current engine matches with US searchDefault from list.json
add_task(async function test_searchDefaultEngineUS() {
  await SearchTestUtils.useTestEngines();

  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );

  Services.prefs.setCharPref(SearchUtils.BROWSER_SEARCH_PREF + "region", "US");

  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();

  Assert.ok(Services.search.isInitialized, "search initialized");

  Assert.equal(
    Services.search.defaultEngine.name,
    "Test search engine",
    "Should have the expected engine as default."
  );
  Assert.equal(
    Services.search.appDefaultEngine.name,
    "Test search engine",
    "Should have the expected engine as the app default"
  );

  // First with the pref off to check using the existing values.
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    false
  );

  Assert.equal(
    Services.search.defaultPrivateEngine.name,
    Services.search.defaultEngine.name,
    "Should have the normal default engine when separate private browsing is off."
  );
  Assert.equal(
    Services.search.appPrivateDefaultEngine.name,
    Services.search.appDefaultEngine.name,
    "Should have the normal app engine when separate private browsing is off."
  );

  // Then with the pref on.
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );

  Assert.equal(
    Services.search.defaultPrivateEngine.name,
    "engine-pref",
    "Should have the private default engine when separate private browsing is on."
  );
  Assert.equal(
    Services.search.appPrivateDefaultEngine.name,
    "engine-pref",
    "Should have the app private engine set correctly when separate private browsing is on."
  );

  Services.prefs.clearUserPref("browser.search.region");
});
