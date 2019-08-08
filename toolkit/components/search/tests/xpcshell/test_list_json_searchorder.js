/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Check default search engine is picked from list.json searchDefault */

"use strict";

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();

  useTestEngineConfig();
});

add_task(async function test_searchOrderJSON() {
  await asyncReInit();

  Assert.ok(Services.search.isInitialized, "search initialized");
  Assert.equal(
    Services.search.defaultEngine.name,
    kTestEngineName,
    "expected test list JSON default search engine"
  );

  let sortedEngines = await Services.search.getEngines();
  Assert.equal(
    sortedEngines[0].name,
    "Test search engine",
    "First engine should be default"
  );
  Assert.equal(
    sortedEngines[1].name,
    "engine-resourceicon",
    "Second engine should be resource"
  );
  Assert.equal(
    sortedEngines[2].name,
    "engine-chromeicon",
    "Third engine should be chrome"
  );
});
