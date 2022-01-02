/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that originalDefaultEngine property is set and switches correctly.
 */

"use strict";

add_task(async function setup() {
  Region._setHomeRegion("an", false);
  await AddonTestUtils.promiseStartupManager();
  await SearchTestUtils.useTestEngines("test-extensions");
});

function promiseDefaultNotification() {
  return SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.DEFAULT,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
}

add_task(async function test_originalDefaultEngine() {
  await Promise.all([Services.search.init(), promiseAfterSettings()]);
  Assert.equal(
    Services.search.originalDefaultEngine.name,
    "Multilocale AN",
    "Should have returned the correct original default engine"
  );
});

add_task(async function test_changeRegion() {
  // Now change the region, and check we get the correct default according to
  // the config file.

  // Note: the test could be done with changing regions or locales. The important
  // part is that the default engine is changing across the switch, and that
  // the engine is not the first one in the new sorted engines list.
  await promiseSetHomeRegion("tr");

  Assert.equal(
    Services.search.originalDefaultEngine.name,
    // Very important this default is not the first one in the list (which is
    // the next fallback if the config one can't be found).
    "Special",
    "Should have returned the correct engine for the new locale"
  );
});
