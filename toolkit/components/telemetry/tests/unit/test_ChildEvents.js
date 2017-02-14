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

const RECORDED_CONTENT_EVENTS = [
  ["telemetry.test", "test1", "object1"],
  ["telemetry.test", "test1", "object1", null, {"key1": "x", "key2": "y"}],
  ["telemetry.test", "test1", "object1", "foo", {"key1": "x", "key2": "y"}],
];

function run_child_test() {
  // Record some events in the "content" process.
  RECORDED_CONTENT_EVENTS.forEach(e => Telemetry.recordEvent(...e));
}

/**
 * This function waits until content events are reported into the
 * events snapshot.
 */
function* waitForContentEvents() {
  yield ContentTaskUtils.waitForCondition(() => {
    const snapshot =
      Telemetry.snapshotBuiltinEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
    return Object.keys(snapshot).includes("tab");
  });
}

add_task(function*() {
  if (!runningInParent) {
    TelemetryController.testSetupContent();
    run_child_test();
    do_send_remote_message(MESSAGE_CHILD_TEST_DONE);
    return;
  }

  // Setup.
  do_get_profile(true);
  loadAddonManager(APP_ID, APP_NAME, APP_VERSION, PLATFORM_VERSION);
  Services.prefs.setBoolPref(PREF_TELEMETRY_ENABLED, true);
  yield TelemetryController.testSetup();
  // Make sure we don't generate unexpected pings due to pref changes.
  yield setEmptyPrefWatchlist();
  // Enable recording for the test event category.
  Telemetry.setEventRecordingEnabled("telemetry.test", true);

  // Run test in child, don't wait for it to finish: just wait for the
  // MESSAGE_CHILD_TEST_DONE.
  run_test_in_child("test_ChildEvents.js");
  yield do_await_remote_message(MESSAGE_CHILD_TEST_DONE);

  // Once events are set by the content process, they don't immediately get
  // sent to the parent process. Wait for the Telemetry IPC Timer to trigger
  // and batch send the data back to the parent process.
  yield waitForContentEvents();

  // Get an "environment-changed" ping rather than a "test-ping", as
  // event measurements are only supported in subsession pings.
  const payload = TelemetrySession.getPayload("environment-change");

  // Validate the event data.
  Assert.ok("processes" in payload, "Should have processes section");
  Assert.ok("parent" in payload.processes, "Should have main process section");
  Assert.ok("events" in payload.processes.parent, "Main process section should have events.");
  Assert.ok("content" in payload.processes, "Should have child process section");
  Assert.ok("events" in payload.processes.content, "Child process section should have events.");

  let events = payload.processes.content.events.map(e => e.slice(1));
  Assert.deepEqual(events, RECORDED_CONTENT_EVENTS, "Should have recorded content events.");

  // Make sure all events are cleared from storage properly.
  let snapshot =
      Telemetry.snapshotBuiltinEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true);
  Assert.equal(Object.keys(snapshot).length, 1, "Should have events from one process.");
  snapshot =
      Telemetry.snapshotBuiltinEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true);
  Assert.equal(Object.keys(snapshot).length, 0, "Should have cleared all events from storage.");
});
