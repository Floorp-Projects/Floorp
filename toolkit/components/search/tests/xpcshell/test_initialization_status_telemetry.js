/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests telemetry is captured when search service initialization has failed or
 * succeeded.
 */
const searchService = Services.search.wrappedJSObject;

add_setup(async () => {
  consoleAllowList.push("#init: failure initializing search:");
  await SearchTestUtils.useTestEngines("simple-engines");
  await AddonTestUtils.promiseStartupManager();
  Services.fog.initializeFOG();
});

add_task(async function test_init_success_telemetry() {
  Assert.equal(
    searchService.isInitialized,
    false,
    "Search Service should not be initialized."
  );

  await Services.search.init();

  Assert.equal(
    searchService.hasSuccessfullyInitialized,
    true,
    "Search Service should have initialized successfully."
  );

  Assert.equal(
    1,
    await Glean.searchService.initializationStatus.success.testGetValue(),
    "Should have incremented init success by one."
  );
});

add_task(async function test_init_failure_telemetry() {
  await startInitFailure("Settings");
  Assert.equal(
    1,
    await Glean.searchService.initializationStatus.failedSettings.testGetValue(),
    "Should have incremented get settings failure by one."
  );

  await startInitFailure("FetchEngines");
  Assert.equal(
    1,
    await Glean.searchService.initializationStatus.failedFetchEngines.testGetValue(),
    "Should have incremented fetch engines failure by one."
  );

  await startInitFailure("LoadEngines");
  Assert.equal(
    1,
    await Glean.searchService.initializationStatus.failedLoadEngines.testGetValue(),
    "Should have incremented load engines failure by one."
  );
});

async function startInitFailure(errorType) {
  searchService.reset();
  searchService.errorToThrowInTest = errorType;

  Assert.equal(
    searchService.isInitialized,
    false,
    "Search Service should not be initialized."
  );

  let regex = new RegExp(
    `Fake ${errorType} error during search service initialization.`
  );

  await Assert.rejects(
    Services.search.init(),
    regex,
    "Should have thrown an error on init."
  );

  await Assert.rejects(
    Services.search.promiseInitialized,
    regex,
    "Should have rejected the promise."
  );

  searchService.errorToThrowInTest = null;
}
