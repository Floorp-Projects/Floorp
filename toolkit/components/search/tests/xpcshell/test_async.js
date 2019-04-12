/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_async() {
  configureToLoadJarEngines();

  Assert.ok(!Services.search.isInitialized);

  let aStatus = await Services.search.init();
  Assert.ok(Components.isSuccessCode(aStatus));
  Assert.ok(Services.search.isInitialized);

  // test engines from dir are not loaded.
  let engines = await Services.search.getEngines();
  Assert.equal(engines.length, 1);

  // test jar engine is loaded ok.
  let engine = Services.search.getEngineByName("bug645970");
  Assert.notEqual(engine, null);

  // Check the hidden engine is not loaded.
  engine = Services.search.getEngineByName("hidden");
  Assert.equal(engine, null);
});
