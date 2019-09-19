/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SEARCH_SERVICE_TOPIC = "browser-search-service";

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_maybereloadengine_update() {
  useTestEngines("geolookup-extensions");

  let reloadObserved = false;
  let obs = (subject, topic, data) => {
    if (data == "engines-reloaded") {
      reloadObserved = true;
    }
  };
  Services.obs.addObserver(obs, SEARCH_SERVICE_TOPIC);

  let initPromise = Services.search.init(true);

  async function cont(requests) {
    await Promise.all([
      initPromise,
      SearchTestUtils.promiseSearchNotification("ensure-known-region-done"),
      promiseAfterCache(),
    ]);

    Assert.ok(
      reloadObserved,
      "Engines should be reloaded during test, because region fetch succeeded"
    );

    let defaultEngine = await Services.search.getDefault();
    Assert.equal(
      defaultEngine._shortName,
      "multilocale-af",
      "Should have update to the new engine with the same name"
    );

    Services.obs.removeObserver(obs, SEARCH_SERVICE_TOPIC);
  }

  await withGeoServer(cont, {
    geoLookupData: { country_code: "FR" },
    preGeolookupPromise: initPromise,
  });
});
