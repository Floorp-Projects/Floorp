/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check the correct default engines are picked from the configuration list,
 * when we have some with the same orderHint, and some without any.
 */

"use strict";

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();

  await SearchTestUtils.useTestEngines(
    "data",
    null,
    (await readJSONFile(do_get_file("data/engines-no-order-hint.json"))).data
  );

  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
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

add_task(async function test_engine_sort_with_non_builtins_sort() {
  await SearchTestUtils.installSearchExtension({ name: "nonbuiltin1" });

  // As we've added an engine, the pref will have been set to true, but
  // we do really want to test the default sort.
  Services.search.wrappedJSObject._settings.setMetaDataAttribute(
    "useSavedOrder",
    false
  );

  const EXPECTED_ORDER = [
    // Default engine.
    "Test search engine",
    // Alphabetical order for the two with orderHint = 1000.
    "engine-chromeicon",
    "engine-rel-searchform-purpose",
    // Alphabetical order for the remaining engines without orderHint.
    "engine-pref",
    "engine-resourceicon",
    "Test search engine (Reordered)",
  ];

  // We should still have the same built-in engines listed.
  await checkOrder("getAppProvidedEngines", EXPECTED_ORDER);

  const expected = [...EXPECTED_ORDER];
  // This is inserted in alphabetical order for the last three.
  expected.splice(expected.length - 1, 0, "nonbuiltin1");
  await checkOrder("getEngines", expected);
});
