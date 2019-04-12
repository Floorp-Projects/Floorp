/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_async_addon() {
  configureToLoadJarEngines();
  installAddonEngine();

  Assert.ok(!Services.search.isInitialized);

  await Services.search.init();
  Assert.ok(Services.search.isInitialized);

  // test the legacy add-on engine is _not_ loaded
  let engines = await Services.search.getEngines();
  Assert.equal(engines.length, 1);
  Assert.equal(Services.search.getEngineByName("addon"), null);
});
