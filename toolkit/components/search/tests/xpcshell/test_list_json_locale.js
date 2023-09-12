/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Check default search engine is picked from list.json searchDefault */

"use strict";

add_setup(async function () {
  await SearchTestUtils.useTestEngines();

  Services.locale.availableLocales = [
    ...Services.locale.availableLocales,
    "de",
    "fr",
  ];

  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );
  Region._setHomeRegion("US", false);
});

add_task(async function test_listJSONlocale() {
  Services.locale.requestedLocales = ["de"];

  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();

  Assert.ok(Services.search.isInitialized, "search initialized");

  let sortedEngines = await Services.search.getEngines();
  Assert.equal(sortedEngines.length, 1, "Should have only one engine");

  Assert.equal(
    Services.search.defaultEngine.name,
    "Test search engine",
    "Should have the correct default engine"
  );
  Assert.equal(
    Services.search.defaultPrivateEngine.name,
    // 'de' only displays google, so we'll be using the same engine as the
    // normal default.
    "Test search engine",
    "Should have the correct private default engine"
  );
});

// Check that switching locale switches search engines
add_task(async function test_listJSONlocaleSwitch() {
  await promiseSetLocale("fr");

  Assert.ok(Services.search.isInitialized, "search initialized");

  let sortedEngines = await Services.search.getEngines();
  Assert.deepEqual(
    sortedEngines.map(e => e.name),
    ["Test search engine", "engine-pref", "engine-resourceicon"],
    "Should have the correct engine list"
  );

  Assert.equal(
    Services.search.defaultEngine.name,
    "Test search engine",
    "Should have the correct default engine"
  );
  Assert.equal(
    Services.search.defaultPrivateEngine.name,
    "engine-pref",
    "Should have the correct private default engine"
  );
});

// Check that region overrides apply
add_task(async function test_listJSONRegionOverride() {
  await promiseSetHomeRegion("RU");

  Assert.ok(Services.search.isInitialized, "search initialized");

  let sortedEngines = await Services.search.getEngines();
  Assert.deepEqual(
    sortedEngines.map(e => e.name),
    ["Test search engine", "engine-pref", "engine-chromeicon"],
    "Should have the correct engine list"
  );

  Assert.equal(
    Services.search.defaultEngine.name,
    "Test search engine",
    "Should have the correct default engine"
  );
  Assert.equal(
    Services.search.defaultPrivateEngine.name,
    "engine-pref",
    "Should have the correct private default engine"
  );
});
