/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/TelemetryEnvironment.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm", this);
Cu.import("resource://gre/modules/PromiseUtils.jsm", this);

function run_test() {
  do_test_pending();
  do_get_profile();
  run_next_test();
}

function isRejected(promise) {
  return new Promise((resolve, reject) => {
    promise.then(() => resolve(false), () => resolve(true));
  });
}

add_task(function* test_initAndShutdown() {
  // Check that init and shutdown work properly.
  TelemetryEnvironment.init();
  yield TelemetryEnvironment.shutdown();
  TelemetryEnvironment.init();
  yield TelemetryEnvironment.shutdown();

  // A double init should be silently handled.
  TelemetryEnvironment.init();
  TelemetryEnvironment.init();

  // getEnvironmentData should return a sane result.
  let data = yield TelemetryEnvironment.getEnvironmentData();
  Assert.ok(!!data);

  // The change listener registration should silently fail after shutdown.
  yield TelemetryEnvironment.shutdown();
  TelemetryEnvironment.registerChangeListener("foo", () => {});
  TelemetryEnvironment.unregisterChangeListener("foo");

  // Other calls after shutdown should reject.
  Assert.ok(yield isRejected(TelemetryEnvironment.shutdown()));
  Assert.ok(yield isRejected(TelemetryEnvironment.getEnvironmentData()));
});

add_task(function* test_changeNotify() {
  TelemetryEnvironment.init();

  // Register some listeners
  let results = new Array(4).fill(false);
  for (let i=0; i<results.length; ++i) {
    let k = i;
    TelemetryEnvironment.registerChangeListener("test"+k, () => results[k] = true);
  }
  // Trigger environment change notifications.
  // TODO: test with proper environment changes, not directly.
  TelemetryEnvironment._onEnvironmentChange("foo");
  Assert.ok(results.every(val => val), "All change listeners should have been notified.");
  results.fill(false);
  TelemetryEnvironment._onEnvironmentChange("bar");
  Assert.ok(results.every(val => val), "All change listeners should have been notified.");

  // Unregister listeners
  for (let i=0; i<4; ++i) {
    TelemetryEnvironment.unregisterChangeListener("test"+i);
  }
});

add_task(function* test_prefWatchPolicies() {
  const PREF_TEST_1 = "toolkit.telemetry.test.pref_new";
  const PREF_TEST_2 = "toolkit.telemetry.test.pref1";
  const PREF_TEST_3 = "toolkit.telemetry.test.pref2";

  const expectedValue = "some-test-value";

  let prefsToWatch = {};
  prefsToWatch[PREF_TEST_1] = TelemetryEnvironment.RECORD_PREF_VALUE;
  prefsToWatch[PREF_TEST_2] = TelemetryEnvironment.RECORD_PREF_STATE;
  prefsToWatch[PREF_TEST_3] = TelemetryEnvironment.RECORD_PREF_STATE;

  yield TelemetryEnvironment.init();

  // Set the Environment preferences to watch.
  TelemetryEnvironment._watchPreferences(prefsToWatch);
  let deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener("testWatchPrefs", deferred.resolve);

  // Trigger a change in the watched preferences.
  Preferences.set(PREF_TEST_1, expectedValue);
  Preferences.set(PREF_TEST_2, false);
  yield deferred.promise;

  // Unregister the listener.
  TelemetryEnvironment.unregisterChangeListener("testWatchPrefs");

  // Check environment contains the correct data.
  let environmentData = yield TelemetryEnvironment.getEnvironmentData();

  let userPrefs = environmentData.settings.userPrefs;

  Assert.equal(userPrefs[PREF_TEST_1], expectedValue,
               "Environment contains the correct preference value.");
  Assert.equal(userPrefs[PREF_TEST_2], null,
               "Report that the pref was user set and has no value.");
  Assert.ok(!(PREF_TEST_3 in userPrefs),
            "Do not report if preference not user set.");

  yield TelemetryEnvironment.shutdown();
});

add_task(function* test_prefWatch_prefReset() {
  const PREF_TEST = "toolkit.telemetry.test.pref1";

  let prefsToWatch = {};
  prefsToWatch[PREF_TEST] = TelemetryEnvironment.RECORD_PREF_STATE;
  // Set the preference to a non-default value.
  Preferences.set(PREF_TEST, false);

  yield TelemetryEnvironment.init();

  // Set the Environment preferences to watch.
  TelemetryEnvironment._watchPreferences(prefsToWatch);
  let deferred = PromiseUtils.defer();
  TelemetryEnvironment.registerChangeListener("testWatchPrefs_reset", deferred.resolve);

  // Trigger a change in the watched preferences.
  Preferences.reset(PREF_TEST);
  yield deferred.promise;

  // Unregister the listener.
  TelemetryEnvironment.unregisterChangeListener("testWatchPrefs_reset");
  yield TelemetryEnvironment.shutdown();
});

add_task(function*() {
  do_test_finished();
});
