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
  ["telemetry.test", "content_only", "object1"],
  ["telemetry.test", "main_and_content", "object1"],
  ["telemetry.test", "content_only", "object1", "some value"],
  ["telemetry.test", "content_only", "object1", null, {foo: "x", bar: "y"}],
  ["telemetry.test", "content_only", "object1", "some value", {foo: "x", bar: "y"}],
];

const UNRECORDED_CONTENT_EVENTS = [
  ["telemetry.test", "main_only", "object1"],
];

const RECORDED_PARENT_EVENTS = [
  ["telemetry.test", "main_and_content", "object1"],
  ["telemetry.test", "main_only", "object1"],
];

const UNRECORDED_PARENT_EVENTS = [
  ["telemetry.test", "content_only", "object1"],
];

function run_child_test() {
  // Record some events in the "content" process.
  RECORDED_CONTENT_EVENTS.forEach(e => Telemetry.recordEvent(...e));
  // These events should not be recorded for the content process.
  UNRECORDED_CONTENT_EVENTS.forEach(e => Telemetry.recordEvent(...e));
}

/**
 * This function waits until content events are reported into the
 * events snapshot.
 */
async function waitForContentEvents() {
  await ContentTaskUtils.waitForCondition(() => {
    const snapshot =
      Telemetry.snapshotBuiltinEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
    return Object.keys(snapshot).includes("content");
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
  finishAddonManagerStartup();
  Services.prefs.setBoolPref(TelemetryUtils.Preferences.TelemetryEnabled, true);
  await TelemetryController.testSetup();
  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();
  // Enable recording for the test event category.
  Telemetry.setEventRecordingEnabled("telemetry.test", true);

  // Run test in child, don't wait for it to finish: just wait for the
  // MESSAGE_CHILD_TEST_DONE.
  const timestampBeforeChildEvents = Telemetry.msSinceProcessStart();
  run_test_in_child("test_ChildEvents.js");
  await do_await_remote_message(MESSAGE_CHILD_TEST_DONE);

  // Once events are set by the content process, they don't immediately get
  // sent to the parent process. Wait for the Telemetry IPC Timer to trigger
  // and batch send the data back to the parent process.
  await waitForContentEvents();
  const timestampAfterChildEvents = Telemetry.msSinceProcessStart();

  // Also record some events in the parent.
  RECORDED_PARENT_EVENTS.forEach(e => Telemetry.recordEvent(...e));
  UNRECORDED_PARENT_EVENTS.forEach(e => Telemetry.recordEvent(...e));

  // Get an "environment-changed" ping rather than a "test-ping", as
  // event measurements are only supported in subsession pings.
  const payload = TelemetrySession.getPayload("environment-change");

  // Validate the event data is present in the payload.
  Assert.ok("processes" in payload, "Should have processes section");
  Assert.ok("parent" in payload.processes, "Should have main process section");
  Assert.ok("events" in payload.processes.parent, "Main process section should have events.");
  Assert.ok("content" in payload.processes, "Should have child process section");
  Assert.ok("events" in payload.processes.content, "Child process section should have events.");

  // Check that the expected events are present from the content process.
  let contentEvents = payload.processes.content.events.map(e => e.slice(1));
  Assert.equal(contentEvents.length, RECORDED_CONTENT_EVENTS.length, "Should match expected event count.");
  for (let i = 0; i < RECORDED_CONTENT_EVENTS.length; ++i) {
    Assert.deepEqual(contentEvents[i], RECORDED_CONTENT_EVENTS[i], "Should have recorded expected event.");
  }

  // Check that the expected events are present from the parent process.
  let parentEvents = payload.processes.parent.events.map(e => e.slice(1));
  Assert.equal(parentEvents.length, RECORDED_PARENT_EVENTS.length, "Should match expected event count.");
  for (let i = 0; i < RECORDED_PARENT_EVENTS.length; ++i) {
    Assert.deepEqual(parentEvents[i], RECORDED_PARENT_EVENTS[i], "Should have recorded expected event.");
  }

  // Check that the event timestamps are in the expected ranges.
  let contentTimestamps = payload.processes.content.events.map(e => e[0]);
  let parentTimestamps = payload.processes.parent.events.map(e => e[0]);

  Assert.ok(contentTimestamps.every(ts => (ts > Math.floor(timestampBeforeChildEvents)) &&
                                          (ts < timestampAfterChildEvents)),
            "All content event timestamps should be in the expected time range.");
  Assert.ok(parentTimestamps.every(ts => (ts >= Math.floor(timestampAfterChildEvents))),
            "All parent event timestamps should be in the expected time range.");

  // Make sure all events are cleared from storage properly.
  let snapshot =
      Telemetry.snapshotBuiltinEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true);
  Assert.greaterOrEqual(Object.keys(snapshot).length, 2, "Should have events from at least two processes.");
  snapshot =
      Telemetry.snapshotBuiltinEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true);
  Assert.equal(Object.keys(snapshot).length, 0, "Should have cleared all events from storage.");
});
