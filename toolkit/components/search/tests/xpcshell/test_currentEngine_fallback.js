/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  Assert.ok(Services.search.getVisibleEngines().length > 1);
  Assert.ok(Services.search.isInitialized);

  // Remove the current engine...
  let currentEngine = Services.search.currentEngine;
  Services.search.removeEngine(currentEngine);

  // ... and verify a new current engine has been set.
  Assert.notEqual(Services.search.currentEngine.name, currentEngine.name);
  Assert.ok(currentEngine.hidden);

  // Remove all the other engines.
  Services.search.getVisibleEngines().forEach(Services.search.removeEngine);
  Assert.equal(Services.search.getVisibleEngines().length, 0);

  // Verify the original default engine is used as a fallback and no
  // longer hidden.
  Assert.equal(Services.search.currentEngine.name, currentEngine.name);
  Assert.ok(!currentEngine.hidden);
  Assert.equal(Services.search.getVisibleEngines().length, 1);
}
