/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_async_distribution() {
  configureToLoadJarEngines();
  installDistributionEngine();

  Assert.ok(!Services.search.isInitialized);

  return Services.search.init().then(function search_initialized(aStatus) {
    Assert.ok(Components.isSuccessCode(aStatus));
    Assert.ok(Services.search.isInitialized);

    // test that the engine from the distribution overrides our jar engine
    return Services.search.getEngines().then(engines => {
      Assert.equal(engines.length, 1);

      let engine = Services.search.getEngineByName("bug645970");
      Assert.notEqual(engine, null);

      // check the engine we have is actually the one from the distribution
      Assert.equal(engine.description, "override");
    });
  });
});
