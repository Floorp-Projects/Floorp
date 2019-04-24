/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SEARCH_SERVICE_TOPIC = "browser-search-service";
const SEARCH_ENGINE_TOPIC = "browser-search-engine-modified";

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_regular_init() {
  await withGeoServer(async function cont(requests) {
    let reloadObserved = false;
    let obs = (subject, topic, data) => {
      if (data == "engines-reloaded") {
        reloadObserved = true;
      }
    };
    Services.obs.addObserver(obs, SEARCH_SERVICE_TOPIC);

    await Promise.all([
      Services.search.init(),
      SearchTestUtils.promiseSearchNotification("ensure-known-region-done"),
      promiseAfterCache(),
    ]);
    // If the system is under load (e.g. xpcshell-tests running in parallel),
    // then we can hit the state where init() is still running, but the cache
    // save fires early. This generates a second `write-cache-to-disk-complete`
    // which can then let us sometimes proceed early. Therefore we have this
    // additional wait to ensure that any cache saves are complete before
    // we move on.
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, 2000));
    checkRequest(requests);

    // Install kTestEngineName and wait for it to reach the disk.
    await Promise.all([installTestEngine(), promiseAfterCache()]);

    Assert.equal(kTestEngineName, (await Services.search.getDefault()).name,
      "Geo defined default should be set");
    checkNoRequest(requests);

    Assert.ok(!reloadObserved, "Engines should not be reloaded during test, because region fetch succeeded");
    Services.obs.removeObserver(obs, SEARCH_SERVICE_TOPIC);
  }, { delay: 100 });
});

add_task(async function test_init_with_skip_regioncheck() {
  await withGeoServer(async function cont(requests) {
    let reloadObserved = false;
    let obs = (subject, topic, data) => {
      if (data == "engines-reloaded") {
        reloadObserved = true;
      }
    };
    Services.obs.addObserver(obs, SEARCH_SERVICE_TOPIC);

    // Break the hash.
    let metadata = await promiseGlobalMetadata();
    metadata.searchDefaultHash = "broken";
    await promiseSaveGlobalMetadata(metadata);

    // Kick off a re-init.
    await asyncReInit();
    let otherPromises = [
      SearchTestUtils.promiseSearchNotification("ensure-known-region-done"),
      promiseAfterCache(),
      SearchTestUtils.promiseSearchNotification("engine-default", SEARCH_ENGINE_TOPIC),
    ];

    // Make sure that the original default engine is put back in place.
    Services.search.resetToOriginalDefaultEngine();
    Assert.notEqual(Services.search.defaultEngine.name, kTestEngineName,
      "Test engine shouldn't be the default anymore");

    await Promise.all(otherPromises);

    checkRequest(requests);
    // Ensure that correct engine is being reported as the default.
    Assert.equal(Services.search.defaultEngine.name, kTestEngineName,
      "Test engine should be the default, because region fetch succeeded");

    Assert.ok(reloadObserved, "Engines should be reloaded during test, because region fetch succeeded");
    Services.obs.removeObserver(obs, SEARCH_SERVICE_TOPIC);
  }, { delay: 100 });
});

add_task(async function test_init_with_skip_regioncheck_no_engine_change() {
  await withGeoServer(async function cont(requests) {
    let reloadObserved = false;
    let reloadObs = (subject, topic, data) => {
      if (data == "engines-reloaded") {
        reloadObserved = true;
      }
    };
    Services.obs.addObserver(reloadObs, SEARCH_SERVICE_TOPIC);
    let engineChanged = false;
    let changeObs = (subject, topic, data) => {
      if (data == "engine-default") {
        engineChanged = true;
      }
    };
    Services.obs.addObserver(changeObs, SEARCH_ENGINE_TOPIC);

    // Break the hash.
    let metadata = await promiseGlobalMetadata();
    metadata.searchDefaultHash = "broken";
    await promiseSaveGlobalMetadata(metadata);

    // Kick off a re-init.
    await Promise.all([asyncReInit(),
      SearchTestUtils.promiseSearchNotification("ensure-known-region-done"),
      promiseAfterCache()]);

    checkRequest(requests);
    // Ensure that correct engine is being reported as the default.
    Assert.equal(Services.search.defaultEngine.name, kTestEngineName,
      "Test engine should be the default, because region fetch succeeded");

    Assert.ok(reloadObserved, "Engines should be reloaded during test, because region fetch succeeded");
    Services.obs.removeObserver(reloadObs, SEARCH_SERVICE_TOPIC);

    Assert.ok(!engineChanged, "Engine should not have changed when a region fetch didn't change it");
    Services.obs.removeObserver(changeObs, SEARCH_ENGINE_TOPIC);
  }, { delay: 100 });
});
