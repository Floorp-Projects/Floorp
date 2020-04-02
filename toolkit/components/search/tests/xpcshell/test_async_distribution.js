/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
  await useTestEngines("simple-engines");
});

add_task(async function test_async_distribution() {
  installDistributionEngine();

  Assert.ok(!Services.search.isInitialized);

  await Services.search.init();
  Assert.ok(Services.search.isInitialized);

  const engines = await Services.search.getEngines();
  Assert.equal(engines.length, 2, "Should have got two engines");

  let engine = Services.search.getEngineByName("basic");
  Assert.ok(engine, "Should have obtained an engine.");

  Assert.equal(
    engine.description,
    "override",
    "Should have got the distribution engine"
  );

  Assert.equal(
    engine.wrappedJSObject._isBuiltin,
    true,
    "Distribution engines should still be marked as built-in"
  );
});
