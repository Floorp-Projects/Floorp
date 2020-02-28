/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Check the correct default engines are picked from the configuration list. */

"use strict";

const modernConfig = Services.prefs.getBoolPref(
  SearchUtils.BROWSER_SEARCH_PREF + "modernConfig",
  false
);

// With modern configuration, we have a slightly different order, since the
// default engines will get place first, regardless of the specified orders
// of the other engines.
const EXPECTED_ORDER = modernConfig
  ? [
      // Default engines
      "Test search engine",
      "engine-pref",
      // Now the engines in orderHint order.
      "engine-resourceicon",
      "engine-chromeicon",
      "engine-rel-searchform-purpose",
      "Test search engine (Reordered)",
    ]
  : [
      // Two engines listed in searchOrder.
      "engine-resourceicon",
      "engine-chromeicon",
      // Rest of the engines in alphabetical order.
      "engine-pref",
      "engine-rel-searchform-purpose",
      "Test search engine",
      "Test search engine (Reordered)",
    ];

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();

  await useTestEngines();

  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );
  // Even though we don't use the distro bits to begin with, we still have
  // to set the pref now, as this gets cached.
  Services.prefs.getDefaultBranch("distribution.").setCharPref("id", "test");
});

async function checkOrder(expectedOrder) {
  const sortedEngines = await Services.search.getDefaultEngines();
  Assert.deepEqual(
    sortedEngines.map(s => s.name),
    expectedOrder,
    "Should have the expected engine order"
  );
}

add_task(async function test_getDefaultEngines_no_others() {
  await checkOrder(EXPECTED_ORDER);
});

add_task(async function test_getDefaultEngines_with_non_builtins() {
  // Reset the sorted list.
  Services.search.wrappedJSObject.__sortedEngines = null;

  await Services.search.addEngineWithDetails("nonbuiltin1", {
    method: "get",
    template: "http://example.com/?search={searchTerms}",
  });

  // We should still have the same built-in engines listed.
  await checkOrder(EXPECTED_ORDER);
});

add_task(async function test_getDefaultEngines_with_distro() {
  // Reset the sorted list.
  Services.search.wrappedJSObject.__sortedEngines = null;

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

  // TODO: Bug 1590860. The order of the lists is wrong - the order prefs should
  // be overriding it, but they don't because the code doesn't work right.
  const expected = modernConfig
    ? [
        "Test search engine",
        "engine-pref",
        "engine-resourceicon",
        "engine-chromeicon",
        "engine-rel-searchform-purpose",
        "Test search engine (Reordered)",
      ]
    : [
        "engine-pref",
        "engine-rel-searchform-purpose",
        "engine-resourceicon",
        "engine-chromeicon",
        "Test search engine",
        "Test search engine (Reordered)",
      ];

  await checkOrder(expected);
});
