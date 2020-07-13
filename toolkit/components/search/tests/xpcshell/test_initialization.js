/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let getStub;

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
  getStub = await useTestEngines("simple-engines");
});

add_task(async function test_init_success() {
  await Services.search.init();
  Assert.ok(Services.search.isInitialized);

  let scalars = Services.telemetry.getSnapshotForScalars("main", true).parent;
  Assert.equal(
    scalars["browser.searchinit.init_result_status_code"],
    Cr.NS_OK,
    "Should have recorded the engines cache as not corrupted"
  );

  await Services.search.init();

  scalars = Services.telemetry.getSnapshotForScalars("main", true).parent;
  Assert.ok(!scalars, "Should not have recorded the scalar a second time");
});

add_task(async function test_initialization_failure() {
  getStub.returns([]);
  delete Services.search.wrappedJSObject._initStarted;
  Services.search.wrappedJSObject._initObservers = PromiseUtils.defer();
  Services.search.wrappedJSObject._initRV = Cr.NS_OK;

  await Assert.rejects(
    Services.search.init(),
    ex => ex.result == Cr.NS_ERROR_UNEXPECTED,
    "Should have failed to initialize"
  );

  let scalars = Services.telemetry.getSnapshotForScalars("main", true).parent;
  Assert.equal(
    scalars["browser.searchinit.init_result_status_code"],
    Cr.NS_ERROR_UNEXPECTED,
    "Should have recorded the unexpected error code"
  );

  await Assert.rejects(
    Services.search.init(),
    result => result == Cr.NS_ERROR_UNEXPECTED,
    "Should have failed to initialize"
  );

  scalars = Services.telemetry.getSnapshotForScalars("main", true).parent;
  Assert.ok(!scalars, "Should not have recorded the scalar a second time");

  sinon.restore();
});
