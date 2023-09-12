/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check the correct default engines are picked from the configuration list,
 * and have the correct orders.
 */

"use strict";

const EXPECTED_ORDER = [
  // Default engines
  "Test search engine",
  "engine-pref",
  // Now the engines in orderHint order.
  "engine-resourceicon",
  "engine-chromeicon",
  "engine-rel-searchform-purpose",
  "Test search engine (Reordered)",
];

add_setup(async function () {
  await AddonTestUtils.promiseStartupManager();

  await SearchTestUtils.useTestEngines();

  Services.locale.availableLocales = [
    ...Services.locale.availableLocales,
    "gd",
  ];

  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );
});

async function checkOrder(type, expectedOrder) {
  // Reset the sorted list.
  Services.search.wrappedJSObject._cachedSortedEngines = null;

  const sortedEngines = await Services.search[type]();
  Assert.deepEqual(
    sortedEngines.map(s => s.name),
    expectedOrder,
    `Should have the expected engine order from ${type}`
  );
}

add_task(async function test_engine_sort_only_builtins() {
  await checkOrder("getAppProvidedEngines", EXPECTED_ORDER);
  await checkOrder("getEngines", EXPECTED_ORDER);
});

add_task(async function test_engine_sort_with_non_builtins_sort() {
  await SearchTestUtils.installSearchExtension({ name: "nonbuiltin1" });

  // As we've added an engine, the pref will have been set to true, but
  // we do really want to test the default sort.
  Services.search.wrappedJSObject._settings.setMetaDataAttribute(
    "useSavedOrder",
    false
  );

  // We should still have the same built-in engines listed.
  await checkOrder("getAppProvidedEngines", EXPECTED_ORDER);

  const expected = [...EXPECTED_ORDER];
  expected.splice(EXPECTED_ORDER.length, 0, "nonbuiltin1");
  await checkOrder("getEngines", expected);
});

add_task(async function test_engine_sort_with_locale() {
  await promiseSetLocale("gd");

  const expected = [
    "engine-resourceicon-gd",
    "engine-pref",
    "engine-rel-searchform-purpose",
    "engine-chromeicon",
    "Test search engine (Reordered)",
  ];

  await checkOrder("getAppProvidedEngines", expected);
  expected.push("nonbuiltin1");
  await checkOrder("getEngines", expected);
});
