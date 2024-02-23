/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test engine order is not set after engine reload.
 */

"use strict";

add_setup(async function () {
  await SearchTestUtils.useTestEngines("test-extensions");
  await AddonTestUtils.promiseStartupManager();

  registerCleanupFunction(AddonTestUtils.promiseShutdownManager);
});

add_task(async function basic_multilocale_test() {
  let resolver;
  let initPromise = new Promise(resolve => (resolver = resolve));
  useCustomGeoServer("TR", initPromise);

  await Services.search.init();
  await Services.search.getAppProvidedEngines();
  resolver();
  await SearchTestUtils.promiseSearchNotification("engines-reloaded");

  let engines = await Services.search.getAppProvidedEngines();

  Assert.deepEqual(
    engines.map(e => e._name),
    ["Special", "Plain"],
    "Special engine is default so should be first"
  );

  engines.forEach(engine => {
    Assert.ok(!engine._metaData.order, "Order is not defined");
  });

  Assert.equal(
    Services.search.wrappedJSObject._settings.getMetaDataAttribute(
      "useSavedOrder"
    ),
    false,
    "We should not set the engine order during maybeReloadEngines"
  );
});
