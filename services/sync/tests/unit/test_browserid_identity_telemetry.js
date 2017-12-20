Cu.import("resource://gre/modules/Preferences.jsm");
let {telemetryHelper} = Cu.import("resource://services-sync/browserid_identity.js", {});

const prefs = new Preferences("services.sync.");

function cleanup() {
  prefs.resetBranch();
}

add_test(function test_success() {
  telemetryHelper.maybeRecordLoginState(telemetryHelper.STATES.SUCCESS);
  Assert.deepEqual(getLoginTelemetryScalar(), {SUCCESS: 1});
  // doing it again should not bump the count.
  telemetryHelper.maybeRecordLoginState(telemetryHelper.STATES.SUCCESS);
  Assert.deepEqual(getLoginTelemetryScalar(), {});

  // check the prefs.
  Assert.equal(prefs.get(telemetryHelper.PREFS.REJECTED_AT), undefined);
  Assert.equal(prefs.get(telemetryHelper.PREFS.APPEARS_PERMANENTLY_REJECTED), undefined);
  Assert.equal(prefs.get(telemetryHelper.PREFS.LAST_RECORDED_STATE), telemetryHelper.STATES.SUCCESS);

  cleanup();
  run_next_test();
});

add_test(function test_success_to_error() {
  telemetryHelper.maybeRecordLoginState(telemetryHelper.STATES.SUCCESS);
  Assert.deepEqual(getLoginTelemetryScalar(), {SUCCESS: 1});
  // transition to an error state
  telemetryHelper.maybeRecordLoginState(telemetryHelper.STATES.NOTVERIFIED);
  Assert.deepEqual(getLoginTelemetryScalar(), {NOTVERIFIED: 1});

  cleanup();
  run_next_test();
});

add_test(function test_unverified() {
  telemetryHelper.nowInMinutes = () => 10000;
  telemetryHelper.maybeRecordLoginState(telemetryHelper.STATES.NOTVERIFIED);
  Assert.deepEqual(getLoginTelemetryScalar(), {NOTVERIFIED: 1});

  Assert.equal(prefs.get(telemetryHelper.PREFS.REJECTED_AT), 10000);

  // doing it again should not bump the count and should not change the
  // original time it was rejected at.
  telemetryHelper.nowInMinutes = () => 10010;
  telemetryHelper.maybeRecordLoginState(telemetryHelper.STATES.NOTVERIFIED);
  Assert.deepEqual(getLoginTelemetryScalar(), {});
  Assert.equal(sumHistogram("WEAVE_LOGIN_FAILED_FOR"), 0);

  Assert.equal(prefs.get(telemetryHelper.PREFS.REJECTED_AT), 10000);

  // record unverified again - should not record telemetry as it's the same state.
  telemetryHelper.maybeRecordLoginState(telemetryHelper.STATES.NOTVERIFIED);
  Assert.deepEqual(getLoginTelemetryScalar(), {});
  Assert.equal(sumHistogram("WEAVE_LOGIN_FAILED_FOR"), 0);

  // now record success - should get the new state recorded and the duration.
  telemetryHelper.maybeRecordLoginState(telemetryHelper.STATES.SUCCESS);
  Assert.deepEqual(getLoginTelemetryScalar(), {SUCCESS: 1});

  // we are now at 10010 minutes, so should have recorded it took 10.
  Assert.equal(sumHistogram("WEAVE_LOGIN_FAILED_FOR"), 10);

  cleanup();
  run_next_test();
});

add_test(function test_unverified_give_up() {
  telemetryHelper.nowInMinutes = () => 10000;
  telemetryHelper.maybeRecordLoginState(telemetryHelper.STATES.NOTVERIFIED);
  Assert.deepEqual(getLoginTelemetryScalar(), {NOTVERIFIED: 1});

  Assert.equal(prefs.get(telemetryHelper.PREFS.REJECTED_AT), 10000);

  // bump our timestamp to 50k minutes, which is past our "give up" period.
  telemetryHelper.nowInMinutes = () => 50000;
  telemetryHelper.maybeRecordLoginState(telemetryHelper.STATES.NOTVERIFIED);

  // We've already recorded the unverified state, so shouldn't again.
  Assert.deepEqual(getLoginTelemetryScalar(), {});
  // But we *should* have recorded how long it failed for even though it still
  // fails.
  Assert.equal(sumHistogram("WEAVE_LOGIN_FAILED_FOR"), 40000);

  // Record failure again - we should *not* have any telemetry recorded.
  telemetryHelper.nowInMinutes = () => 50001;
  telemetryHelper.maybeRecordLoginState(telemetryHelper.STATES.NOTVERIFIED);
  Assert.deepEqual(getLoginTelemetryScalar(), {});
  Assert.equal(sumHistogram("WEAVE_LOGIN_FAILED_FOR"), 0);

  // Now record success - it also should *not* again record the failure duration.
  telemetryHelper.nowInMinutes = () => 60000;
  telemetryHelper.maybeRecordLoginState(telemetryHelper.STATES.SUCCESS);
  Assert.deepEqual(getLoginTelemetryScalar(), {SUCCESS: 1});
  Assert.equal(sumHistogram("WEAVE_LOGIN_FAILED_FOR"), 0);

  // Even though we were permanently rejected, the SUCCESS recording should
  // have reset that state, so new error states should work as normal.
  telemetryHelper.maybeRecordLoginState(telemetryHelper.STATES.NOTVERIFIED);
  Assert.deepEqual(getLoginTelemetryScalar(), {NOTVERIFIED: 1});
  Assert.equal(sumHistogram("WEAVE_LOGIN_FAILED_FOR"), 0);

  telemetryHelper.nowInMinutes = () => 60001;
  telemetryHelper.maybeRecordLoginState(telemetryHelper.STATES.SUCCESS);
  Assert.deepEqual(getLoginTelemetryScalar(), {SUCCESS: 1});
  Assert.equal(sumHistogram("WEAVE_LOGIN_FAILED_FOR"), 1);

  cleanup();
  run_next_test();
});

add_test(function test_bad_state() {
  // We call the internal implementation to check it throws as the public
  // method catches and logs.
  Assert.throws(() => telemetryHelper._maybeRecordLoginState("foo"), /invalid state/);
  cleanup();
  run_next_test();
});
