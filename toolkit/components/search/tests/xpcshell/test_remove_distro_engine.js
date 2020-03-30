/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
  await useTestEngines("simple-engines");

  installDistributionEngine();
});

add_task(async function test_remove_distro_engine_marks_as_hidden() {
  await Services.search.init();

  let engine = Services.search.getEngineByName("basic");
  Assert.ok(engine, "Should have loaded the engine correctly.");

  Assert.equal(
    engine.description,
    "override",
    "Should have got the distribution engine"
  );

  Assert.ok(!engine.hidden, "Should not be hidden");

  await Services.search.removeEngine(engine);

  // Re-obtain the engine.
  engine = Services.search.getEngineByName("basic");

  Assert.ok(engine, "Should still be available");
  Assert.equal(
    engine.description,
    "override",
    "Should have got the distribution engine"
  );
  Assert.ok(engine.hidden, "Should now be hidden");
});
