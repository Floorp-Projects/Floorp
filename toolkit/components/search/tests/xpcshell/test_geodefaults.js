/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gServerCohort = "";

const kUrlPref = "geoSpecificDefaults.url";

const kDayInSeconds = 86400;
const kYearInSeconds = kDayInSeconds * 365;

add_task(async function no_request_if_prefed_off() {
  await withGeoServer(async function cont(requests) {
    // Disable geoSpecificDefaults and check no HTTP request is made.
    Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", false);
    await AddonTestUtils.promiseStartupManager();
    await Services.search.init();
    await promiseAfterCache();

    checkNoRequest(requests);

    // Install kTestEngineName and wait for it to reach the disk.
    await Promise.all([installTestEngine(), promiseAfterCache()]);

    // The default engine should be set based on the prefs.
    Assert.equal((await Services.search.getDefault()).name, getDefaultEngineName(false));

    // Ensure nothing related to geoSpecificDefaults has been written in the metadata.
    let metadata = await promiseGlobalMetadata();
    Assert.equal(typeof metadata.searchDefaultExpir, "undefined");
    Assert.equal(typeof metadata.searchDefault, "undefined");
    Assert.equal(typeof metadata.searchDefaultHash, "undefined");

    Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", true);
  });
});

add_task(async function should_get_geo_defaults_only_once() {
  await withGeoServer(async function cont(requests) {
    // (Re)initializing the search service should trigger a request,
    // and set the default engine based on it.
    // Due to the previous initialization, we expect the region to already be set.
    Assert.ok(Services.prefs.prefHasUserValue("browser.search.region"));
    Assert.equal(Services.prefs.getCharPref("browser.search.region"), "FR");
    await Promise.all([asyncReInit({ waitForRegionFetch: true }), promiseAfterCache()]);
    checkRequest(requests);
    Assert.equal((await Services.search.getDefault()).name, kTestEngineName);

    // Verify the metadata was written correctly.
    let metadata = await promiseGlobalMetadata();
    Assert.equal(typeof metadata.searchDefaultExpir, "number");
    Assert.ok(metadata.searchDefaultExpir > Date.now());
    Assert.equal(typeof metadata.searchDefault, "string");
    Assert.equal(metadata.searchDefault, "Test search engine");
    Assert.equal(typeof metadata.searchDefaultHash, "string");
    Assert.equal(metadata.searchDefaultHash.length, 44);

    // The next restart shouldn't trigger a request.
    await asyncReInit({ waitForRegionFetch: true });
    checkNoRequest(requests);
    Assert.equal((await Services.search.getDefault()).name, kTestEngineName);
  });
});

add_task(async function should_request_when_region_not_set() {
  await withGeoServer(async function cont(requests) {
    Services.prefs.clearUserPref("browser.search.region");
    await Promise.all([asyncReInit({ waitForRegionFetch: true }), promiseAfterCache()]);
    checkRequest(requests);
  });
});

add_task(async function should_recheck_if_interval_expired() {
  await withGeoServer(async function cont(requests) {
    await forceExpiration();

    let date = Date.now();
    await Promise.all([asyncReInit({ waitForRegionFetch: true }), promiseAfterCache()]);
    checkRequest(requests);

    // Check that the expiration timestamp has been updated.
    let metadata = await promiseGlobalMetadata();
    Assert.equal(typeof metadata.searchDefaultExpir, "number");
    Assert.ok(metadata.searchDefaultExpir >= date + kYearInSeconds * 1000);
    Assert.ok(metadata.searchDefaultExpir < date + (kYearInSeconds + 3600) * 1000);
  });
});

add_task(async function should_recheck_if_appversion_changed() {
  await withGeoServer(async function cont(requests) {
    let data = await promiseCacheData();

    Assert.equal(data.appVersion, Services.appinfo.version);

    // Set app version to something old
    data.appVersion = "1";
    await promiseSaveCacheData(data);

    await Promise.all([asyncReInit({ waitForRegionFetch: true }), promiseAfterCache()]);
    checkRequest(requests);

    // Check that the appVersion has been updated
    data = await promiseCacheData();
    Assert.equal(data.appVersion, Services.appinfo.version);
  });
});

add_task(async function should_recheck_when_broken_hash() {
  await withGeoServer(async function cont(requests) {
    // This test verifies both that we ignore saved geo-defaults if the
    // hash is invalid, and that we keep the local preferences-based
    // default for all of the session.

    let metadata = await promiseGlobalMetadata();

    // Break the hash.
    let hash = metadata.searchDefaultHash;
    metadata.searchDefaultHash = "broken";
    await promiseSaveGlobalMetadata(metadata);

    let commitPromise = promiseAfterCache();
    let unInitPromise = SearchTestUtils.promiseSearchNotification("uninit-complete");
    let reInitPromise = asyncReInit({ waitForRegionFetch: true });
    await unInitPromise;

    await reInitPromise;
    checkRequest(requests);
    await commitPromise;

    // Check that the hash is back to its previous value.
    metadata = await promiseGlobalMetadata();
    Assert.equal(typeof metadata.searchDefaultHash, "string");
    if (metadata.searchDefaultHash == "broken") {
      // If the server takes more than 1000ms to return the result,
      // the commitPromise was resolved by a first save of the cache
      // that saved the engines, but not the request's results.
      info("waiting for the cache to be saved a second time");
      await promiseAfterCache();
      metadata = await promiseGlobalMetadata();
    }
    Assert.equal(metadata.searchDefaultHash, hash);

    // After another restart, the current engine should be back to the geo default,
    // without doing yet another request.
    await asyncReInit({ waitForRegionFetch: true });
    checkNoRequest(requests);
    Assert.equal((await Services.search.getDefault()).name, kTestEngineName);
  });
});

add_task(async function should_remember_cohort_id() {
  const cohortPref = "browser.search.cohort";
  // Make the server send a cohort id.
  const cohort = "xpcshell";

  await withGeoServer(async function cont(requests) {
    // Check that initially the cohort pref doesn't exist.
    Assert.equal(Services.prefs.getPrefType(cohortPref), Services.prefs.PREF_INVALID);

    // Trigger a new request.
    await forceExpiration();
    let commitPromise = promiseAfterCache();
    await asyncReInit({ waitForRegionFetch: true });
    checkRequest(requests);
    await commitPromise;

    // Check that the cohort was saved.
    Assert.equal(Services.prefs.getPrefType(cohortPref), Services.prefs.PREF_STRING);
    Assert.equal(Services.prefs.getCharPref(cohortPref), cohort);
  }, {cohort});

  // Make the server stop sending the cohort.
  await withGeoServer(async function cont(requests) {
    // Check that the next request sends the previous cohort id, and
    // will remove it from the prefs due to the server no longer sending it.
    await forceExpiration();
    let commitPromise = promiseAfterCache();
    await asyncReInit({ waitForRegionFetch: true });
    checkRequest(requests, cohort);
    await commitPromise;
    Assert.equal(Services.prefs.getPrefType(cohortPref), Services.prefs.PREF_INVALID);
  });
});

add_task(async function should_retry_after_failure() {
  await withGeoServer(async function cont(requests) {
    // Trigger a new request.
    await forceExpiration();
    await asyncReInit({ waitForRegionFetch: true });
    checkRequest(requests);

    // After another restart, a new request should be triggered automatically without
    // the test having to call forceExpiration again.
    await asyncReInit({ waitForRegionFetch: true });
    checkRequest(requests);
  }, {path: "lookup_fail"});
});

add_task(async function should_honor_retry_after_header() {
  await withGeoServer(async function cont(requests) {
    // Trigger a new request.
    await forceExpiration();
    let date = Date.now();
    let commitPromise = promiseAfterCache();
    await asyncReInit({ waitForRegionFetch: true });
    await commitPromise;
    checkRequest(requests);

    // Check that the expiration timestamp has been updated.
    let metadata = await promiseGlobalMetadata();
    Assert.equal(typeof metadata.searchDefaultExpir, "number");
    Assert.ok(metadata.searchDefaultExpir >= date + kDayInSeconds * 1000);
    Assert.ok(metadata.searchDefaultExpir < date + (kDayInSeconds + 3600) * 1000);

    // After another restart, a new request should not be triggered.
    await asyncReInit({ waitForRegionFetch: true });
    checkNoRequest(requests);
  }, {path: "lookup_unavailable"});
});
