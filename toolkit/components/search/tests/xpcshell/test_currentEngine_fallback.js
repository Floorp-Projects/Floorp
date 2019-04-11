/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_fallbacks() {
  Assert.ok((await Services.search.getVisibleEngines()).length > 1);
  Assert.ok(Services.search.isInitialized);

  // Remove the current engine...
  let defaultEngine = await Services.search.getDefault();
  await Services.search.removeEngine(defaultEngine);

  // ... and verify a new current engine has been set.
  Assert.notEqual((await Services.search.getDefault()).name, defaultEngine.name);
  Assert.ok(defaultEngine.hidden);

  // Remove all the other engines.
  for (let engine of await Services.search.getVisibleEngines()) {
    await Services.search.removeEngine(engine);
  }
  Assert.strictEqual((await Services.search.getVisibleEngines()).length, 0);

  // Verify the original default engine is used as a fallback and no
  // longer hidden.
  Assert.equal((await Services.search.getDefault()).name, defaultEngine.name);
  Assert.ok(!defaultEngine.hidden);
  Assert.equal((await Services.search.getVisibleEngines()).length, 1);
});
