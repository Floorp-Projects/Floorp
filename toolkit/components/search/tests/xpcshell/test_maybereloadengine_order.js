/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_CONFIG = [
  {
    webExtension: { id: "plainengine@search.mozilla.org" },
    appliesTo: [{ included: { everywhere: true } }],
  },
  {
    webExtension: { id: "special-engine@search.mozilla.org" },
    appliesTo: [{ default: "yes", included: { regions: ["FR"] } }],
  },
];

add_task(async function setup() {
  Services.prefs.setBoolPref("browser.search.gModernConfig", true);
  Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", true);

  await useTestEngines("test-extensions", null, TEST_CONFIG);
  await AddonTestUtils.promiseStartupManager();

  registerCleanupFunction(AddonTestUtils.promiseShutdownManager);
});

add_task(async function basic_multilocale_test() {
  let resolver;
  let initPromise = new Promise(resolve => (resolver = resolve));
  useCustomGeoServer("FR", initPromise);

  await Services.search.init();
  await Services.search.getDefaultEngines();
  resolver();
  await SearchTestUtils.promiseSearchNotification("engines-reloaded");

  let engines = await Services.search.getDefaultEngines();

  Assert.deepEqual(
    engines.map(e => e._name),
    ["Special", "Plain"],
    "Special engine is default so should be first"
  );

  engines.forEach(engine => {
    Assert.ok(!engine._metaData.order, "Order is not defined");
  });

  let useDBForOrder = Services.prefs.getBoolPref(
    `${SearchUtils.BROWSER_SEARCH_PREF}useDBForOrder`,
    false
  );
  Assert.ok(
    !useDBForOrder,
    "We should not set the engine order during maybeReloadEngines"
  );
});
