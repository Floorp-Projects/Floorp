/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_async_distribution() {
  configureToLoadJarEngines();
  installDistributionEngine();

  Assert.ok(!Services.search.isInitialized);

  let aStatus = await Services.search.init();
  Assert.ok(Components.isSuccessCode(aStatus));
  Assert.ok(Services.search.isInitialized);

  // test that the engine from the distribution overrides our jar engine
  let engines = await Services.search.getEngines();
  Assert.equal(engines.length, 1);

  let engine = Services.search.getEngineByName("bug645970");
  Assert.ok(!!engine, "engine is installed");

  // check the engine we have is actually the one from the distribution
  Assert.equal(
    engine.description,
    "override",
    "distribution engine override installed"
  );
});
