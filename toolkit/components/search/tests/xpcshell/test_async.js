/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function () {
  await AddonTestUtils.promiseStartupManager();
  await SearchTestUtils.useTestEngines("simple-engines");
  Services.fog.initializeFOG();
});

add_task(async function test_async() {
  Assert.ok(!Services.search.isInitialized);

  let aStatus = await Services.search.init();
  Assert.ok(Components.isSuccessCode(aStatus));
  Assert.ok(Services.search.isInitialized);

  // test engines from dir are not loaded.
  let engines = await Services.search.getEngines();
  Assert.equal(engines.length, 2);

  // test jar engine is loaded ok.
  let engine = Services.search.getEngineByName("basic");
  Assert.notEqual(engine, null);
  Assert.ok(engine.isAppProvided, "Should be shown as an app-provided engine");

  engine = Services.search.getEngineByName("Simple Engine");
  Assert.notEqual(engine, null);
  Assert.ok(engine.isAppProvided, "Should be shown as an app-provided engine");

  // Check the hidden engine is not loaded.
  engine = Services.search.getEngineByName("hidden");
  Assert.equal(engine, null);

  // Check if there is a value for startup_time
  Assert.notEqual(
    await Glean.searchService.startupTime.testGetValue(),
    undefined,
    "Should have a value stored in startup_time"
  );
});
