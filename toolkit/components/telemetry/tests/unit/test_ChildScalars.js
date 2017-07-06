/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/TelemetryController.jsm", this);
Cu.import("resource://gre/modules/TelemetrySession.jsm", this);
Cu.import("resource://gre/modules/PromiseUtils.jsm", this);
Cu.import("resource://testing-common/ContentTaskUtils.jsm", this);

const MESSAGE_CHILD_TEST_DONE = "ChildTest:Done";

const PLATFORM_VERSION = "1.9.2";
const APP_VERSION = "1";
const APP_ID = "xpcshell@tests.mozilla.org";
const APP_NAME = "XPCShell";

const UINT_SCALAR = "telemetry.test.unsigned_int_kind";
const KEYED_UINT_SCALAR = "telemetry.test.keyed_unsigned_int";
const KEYED_BOOL_SCALAR = "telemetry.test.keyed_boolean_kind";
const CONTENT_ONLY_UINT_SCALAR = "telemetry.test.content_only_uint";
const ALL_PROCESSES_UINT_SCALAR = "telemetry.test.all_processes_uint";
const ALL_CHILD_PROCESSES_STRING_SCALAR = "telemetry.test.all_child_processes_string";

function run_child_test() {
  // Attempt to set some scalar values from the "content" process.
  // The next scalars are not allowed to be recorded in the content process.
  Telemetry.scalarSet(UINT_SCALAR, 1);
  Telemetry.keyedScalarSet(KEYED_UINT_SCALAR, "should-not-be-recorded", 1);

  // The next scalars shou be recorded in only the content process.
  Telemetry.scalarSet(CONTENT_ONLY_UINT_SCALAR, 37);
  Telemetry.scalarSet(ALL_CHILD_PROCESSES_STRING_SCALAR, "all-child-processes");

  // The next scalar will be recorded in the parent and content processes.
  Telemetry.keyedScalarSet(KEYED_BOOL_SCALAR, "content-key", true);
  Telemetry.keyedScalarSet(KEYED_BOOL_SCALAR, "content-key2", false);
  Telemetry.scalarSet(ALL_PROCESSES_UINT_SCALAR, 37);
}

function setParentScalars() {
  // The following scalars are not allowed to be recorded in the parent process.
  Telemetry.scalarSet(CONTENT_ONLY_UINT_SCALAR, 15);
  Telemetry.scalarSet(ALL_CHILD_PROCESSES_STRING_SCALAR, "all-child-processes");

  // The next ones will be recorded only in the parent.
  Telemetry.scalarSet(UINT_SCALAR, 15);

  // This last batch will be available both in the parent and child processes.
  Telemetry.keyedScalarSet(KEYED_BOOL_SCALAR, "parent-key", false);
  Telemetry.scalarSet(ALL_PROCESSES_UINT_SCALAR, 37);
}

function checkParentScalars(processData) {
  const scalars = processData.scalars;
  const keyedScalars = processData.keyedScalars;

  // Check the plain scalars, make sure we're only recording what we expect.
  Assert.ok(!(CONTENT_ONLY_UINT_SCALAR in scalars),
            "Scalars must not be recorded in other processes unless allowed.");
  Assert.ok(!(ALL_CHILD_PROCESSES_STRING_SCALAR in scalars),
            "Scalars must not be recorded in other processes unless allowed.");
  Assert.ok(UINT_SCALAR in scalars,
            `${UINT_SCALAR} must be recorded in the parent process.`);
  Assert.equal(scalars[UINT_SCALAR], 15,
               `${UINT_SCALAR} must have the correct value (parent process).`);
  Assert.ok(ALL_PROCESSES_UINT_SCALAR in scalars,
            `${ALL_PROCESSES_UINT_SCALAR} must be recorded in the parent process.`);
  Assert.equal(scalars[ALL_PROCESSES_UINT_SCALAR], 37,
               `${ALL_PROCESSES_UINT_SCALAR} must have the correct value (parent process).`);

  // Now check the keyed scalars.
  Assert.ok(KEYED_BOOL_SCALAR in keyedScalars,
            `${KEYED_BOOL_SCALAR} must be recorded in the parent process.`);
  Assert.ok("parent-key" in keyedScalars[KEYED_BOOL_SCALAR],
            `${KEYED_BOOL_SCALAR} must be recorded in the parent process.`);
  Assert.equal(Object.keys(keyedScalars[KEYED_BOOL_SCALAR]).length, 1,
            `${KEYED_BOOL_SCALAR} must only contain the expected key in parent process.`);
  Assert.equal(keyedScalars[KEYED_BOOL_SCALAR]["parent-key"], false,
            `${KEYED_BOOL_SCALAR} must have the correct value (parent process).`);
}

function checkContentScalars(processData) {
  const scalars = processData.scalars;
  const keyedScalars = processData.keyedScalars;

  // Check the plain scalars for the content process.
  Assert.ok(!(UINT_SCALAR in scalars),
            "Scalars must not be recorded in other processes unless allowed.");
  Assert.ok(!(KEYED_UINT_SCALAR in keyedScalars),
            "Keyed scalars must not be recorded in other processes unless allowed.");
  Assert.ok(CONTENT_ONLY_UINT_SCALAR in scalars,
            `${CONTENT_ONLY_UINT_SCALAR} must be recorded in the content process.`);
  Assert.equal(scalars[CONTENT_ONLY_UINT_SCALAR], 37,
            `${CONTENT_ONLY_UINT_SCALAR} must have the correct value (content process).`);
  Assert.ok(ALL_CHILD_PROCESSES_STRING_SCALAR in scalars,
            `${ALL_CHILD_PROCESSES_STRING_SCALAR} must be recorded in the content process.`);
  Assert.equal(scalars[ALL_CHILD_PROCESSES_STRING_SCALAR], "all-child-processes",
            `${ALL_CHILD_PROCESSES_STRING_SCALAR} must have the correct value (content process).`);
  Assert.ok(ALL_PROCESSES_UINT_SCALAR in scalars,
            `${ALL_PROCESSES_UINT_SCALAR} must be recorded in the content process.`);
  Assert.equal(scalars[ALL_PROCESSES_UINT_SCALAR], 37,
            `${ALL_PROCESSES_UINT_SCALAR} must have the correct value (content process).`);

  // Check the keyed scalars.
  Assert.ok(KEYED_BOOL_SCALAR in keyedScalars,
            `${KEYED_BOOL_SCALAR} must be recorded in the content process.`);
  Assert.ok("content-key" in keyedScalars[KEYED_BOOL_SCALAR],
            `${KEYED_BOOL_SCALAR} must be recorded in the content process.`);
  Assert.ok("content-key2" in keyedScalars[KEYED_BOOL_SCALAR],
            `${KEYED_BOOL_SCALAR} must be recorded in the content process.`);
  Assert.equal(keyedScalars[KEYED_BOOL_SCALAR]["content-key"], true,
            `${KEYED_BOOL_SCALAR} must have the correct value (content process).`);
  Assert.equal(keyedScalars[KEYED_BOOL_SCALAR]["content-key2"], false,
            `${KEYED_BOOL_SCALAR} must have the correct value (content process).`);
  Assert.equal(Object.keys(keyedScalars[KEYED_BOOL_SCALAR]).length, 2,
            `${KEYED_BOOL_SCALAR} must contain the expected keys in content process.`);
}

/**
 * This function waits until content scalars are reported into the
 * scalar snapshot.
 */
async function waitForContentScalars() {
  await ContentTaskUtils.waitForCondition(() => {
    const scalars =
      Telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);
    return Object.keys(scalars).includes("content");
  });
}

add_task(async function() {
  if (!runningInParent) {
    TelemetryController.testSetupContent();
    run_child_test();
    do_send_remote_message(MESSAGE_CHILD_TEST_DONE);
    return;
  }

  // Setup.
  do_get_profile(true);
  loadAddonManager(APP_ID, APP_NAME, APP_VERSION, PLATFORM_VERSION);
  Services.prefs.setBoolPref(TelemetryUtils.Preferences.TelemetryEnabled, true);
  await TelemetryController.testSetup();
  if (runningInParent) {
    setParentScalars();
    // Make sure we don't generate unexpected pings due to pref changes.
    await setEmptyPrefWatchlist();
  }

  // Run test in child, don't wait for it to finish: just wait for the
  // MESSAGE_CHILD_TEST_DONE.
  run_test_in_child("test_ChildScalars.js");
  await do_await_remote_message(MESSAGE_CHILD_TEST_DONE);

  // Once scalars are set by the content process, they don't immediately get
  // sent to the parent process. Wait for the Telemetry IPC Timer to trigger
  // and batch send the data back to the parent process.
  await waitForContentScalars();

  // Get an "environment-changed" ping rather than a "test-ping", as
  // scalar measurements are only supported in subsession pings.
  const payload = TelemetrySession.getPayload("environment-change");

  // Validate the scalar data.
  Assert.ok("processes" in payload, "Should have processes section");
  Assert.ok("content" in payload.processes, "Should have child process section");
  Assert.ok("scalars" in payload.processes.content, "Child process section should have scalars.");
  Assert.ok("keyedScalars" in payload.processes.content, "Child process section should have keyed scalars.");
  checkParentScalars(payload.processes.parent);
  checkContentScalars(payload.processes.content);

  do_test_finished();
});
