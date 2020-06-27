/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SEARCH_SERVICE_TOPIC = "browser-search-service";
const SEARCH_ENGINE_TOPIC = "browser-search-engine-modified";

add_task(async function setup() {
  Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", true);
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_regular_init() {
  await withGeoServer(
    async function cont(requests) {
      Region._setHomeRegion("us", false);
      await Services.search.init();
      await promiseAfterCache();

      Assert.notEqual(
        Services.search.defaultEngine.name,
        kTestEngineName,
        "Test engine shouldn't be the default"
      );

      // Install kTestEngineName and wait for it to reach the disk.
      await Promise.all([installTestEngine(), promiseAfterCache()]);

      let enginesReloaded = SearchTestUtils.promiseSearchNotification(
        "engines-reloaded"
      );
      Region._setHomeRegion("FR");
      await promiseAfterCache();
      await enginesReloaded;

      Assert.equal(
        kTestEngineName,
        (await Services.search.getDefault()).name,
        "Geo defined default should be set"
      );
    },
    { delay: 100 }
  );
});
