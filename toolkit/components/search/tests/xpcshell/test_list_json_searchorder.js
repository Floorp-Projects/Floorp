/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Check default search engine is picked from list.json searchDefault */

"use strict";

add_setup(async function () {
  await AddonTestUtils.promiseStartupManager();

  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );

  await SearchTestUtils.useTestEngines();
  await Services.search.init();
});

async function checkOrder(expectedOrder) {
  const sortedEngines = await Services.search.getEngines();
  Assert.deepEqual(
    sortedEngines.map(s => s.name),
    expectedOrder,
    "Should have the expected engine order"
  );
}

add_task(async function test_searchOrderJSON_no_separate_private() {
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    false
  );

  await checkOrder([
    // Default engine
    "Test search engine",
    // Two engines listed in searchOrder.
    "engine-resourceicon",
    "engine-chromeicon",
    // Rest of the engines in order.
    "engine-pref",
    "engine-rel-searchform-purpose",
    "Test search engine (Reordered)",
  ]);
});

add_task(async function test_searchOrderJSON_separate_private() {
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );

  await checkOrder([
    // Default engine
    "Test search engine",
    // Default private engine
    "engine-pref",
    // Two engines listed in searchOrder.
    "engine-resourceicon",
    "engine-chromeicon",
    // Rest of the engines in order.
    "engine-rel-searchform-purpose",
    "Test search engine (Reordered)",
  ]);
});
