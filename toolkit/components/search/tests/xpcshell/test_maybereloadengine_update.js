/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SEARCH_SERVICE_TOPIC = "browser-search-service";

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
  await useTestEngines("data", "geolookup-extensions");
});

add_task(async function test_maybereloadengine_update() {
  await Services.search.init();

  let defaultEngine = await Services.search.getDefault();
  Assert.equal(
    defaultEngine._shortName,
    "multilocale-an",
    "Should have update to the new engine with the same name"
  );

  let enginesReloaded = SearchTestUtils.promiseSearchNotification(
    "engines-reloaded"
  );
  Region._setHomeRegion("FR", true);
  await enginesReloaded;

  Assert.equal(
    defaultEngine._shortName,
    "multilocale-af",
    "Should have update to the new engine with the same name"
  );
});
