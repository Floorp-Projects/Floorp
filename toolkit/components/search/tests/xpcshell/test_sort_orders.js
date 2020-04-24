/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check the correct default engines are picked from the configuration list,
 * and have the correct orders.
 */

"use strict";

const SEARCH_PREF = SearchUtils.BROWSER_SEARCH_PREF;

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

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();

  await useTestEngines();

  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );
  // Even though we don't use the distro bits to begin with, we still have
  // to set the pref now, as this gets cached.
  if (!gModernConfig) {
    Services.prefs.getDefaultBranch("distribution.").setCharPref("id", "test");
  }
});

async function checkOrder(type, expectedOrder) {
  // Reset the sorted list.
  Services.search.wrappedJSObject.__sortedEngines = null;

  const sortedEngines = await Services.search[type]();
  Assert.deepEqual(
    sortedEngines.map(s => s.name),
    expectedOrder,
    `Should have the expected engine order from ${type}`
  );
}

add_task(async function test_engine_sort_only_builtins() {
  await checkOrder("getDefaultEngines", EXPECTED_ORDER);
  await checkOrder("getEngines", EXPECTED_ORDER);
});

add_task(async function test_engine_sort_with_non_builtins_sort() {
  await Services.search.addEngineWithDetails("nonbuiltin1", {
    method: "get",
    template: "http://example.com/?search={searchTerms}",
  });

  // As we've added an engine, the pref will have been set to true, but
  // we do really want to test the default sort.
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "useDBForOrder",
    false
  );

  // We should still have the same built-in engines listed.
  await checkOrder("getDefaultEngines", EXPECTED_ORDER);

  const expected = [...EXPECTED_ORDER];
  // For modern config, all the engines in this config specify an order hint,
  // so our added engine gets sorted to the end.
  expected.splice(gModernConfig ? EXPECTED_ORDER.length : 5, 0, "nonbuiltin1");
  await checkOrder("getEngines", expected);
});

add_task(async function test_engine_sort_with_distro() {
  if (gModernConfig) {
    return;
  }
  Services.prefs.setCharPref(
    SearchUtils.BROWSER_SEARCH_PREF + "order.extra.bar",
    "engine-pref"
  );
  Services.prefs.setCharPref(
    SearchUtils.BROWSER_SEARCH_PREF + "order.extra.foo",
    "engine-resourceicon"
  );
  let localizedStr = Cc["@mozilla.org/pref-localizedstring;1"].createInstance(
    Ci.nsIPrefLocalizedString
  );
  localizedStr.data = "engine-rel-searchform-purpose";
  Services.prefs.setComplexValue(
    SearchUtils.BROWSER_SEARCH_PREF + "order.1",
    Ci.nsIPrefLocalizedString,
    localizedStr
  );
  localizedStr = Cc["@mozilla.org/pref-localizedstring;1"].createInstance(
    Ci.nsIPrefLocalizedString
  );
  localizedStr.data = "engine-chromeicon";
  Services.prefs.setComplexValue(
    SearchUtils.BROWSER_SEARCH_PREF + "order.2",
    Ci.nsIPrefLocalizedString,
    localizedStr
  );

  const expected = [
    "engine-pref",
    "engine-resourceicon",
    "engine-rel-searchform-purpose",
    "engine-chromeicon",
    "Test search engine",
    "Test search engine (Reordered)",
  ];

  await checkOrder("getDefaultEngines", expected);

  // For modern config, all the engines in this config specify an order hint,
  // so our added engine gets sorted to the end.
  expected.splice(5, 0, "nonbuiltin1");

  await checkOrder("getEngines", expected);

  Services.prefs.clearUserPref(`${SEARCH_PREF}order.extra.bar`);
  Services.prefs.clearUserPref(`${SEARCH_PREF}order.extra.foo`);
  Services.prefs.clearUserPref(`${SEARCH_PREF}order.1`);
  Services.prefs.clearUserPref(`${SEARCH_PREF}order.2`);
});

add_task(async function test_engine_sort_with_locale() {
  if (!gModernConfig) {
    return;
  }
  Services.locale.availableLocales = ["gd"];
  Services.locale.requestedLocales = ["gd"];

  const expected = [
    "engine-resourceicon-gd",
    "engine-pref",
    "engine-rel-searchform-purpose",
    "engine-chromeicon",
    "Test search engine (Reordered)",
  ];

  await asyncReInit();
  await checkOrder("getDefaultEngines", expected);
  await checkOrder("getEngines", expected);
});
