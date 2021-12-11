/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

const { ContentTaskUtils } = ChromeUtils.import(
  "resource://testing-common/ContentTaskUtils.jsm"
);

const MESSAGE_CHILD_TEST_DONE = "ChildTest:Done";

const TEST_STATIC_EVENT_NAME = "telemetry.test";
const TEST_EVENT_NAME = "telemetry.test.child";

function run_child_test() {
  Telemetry.recordEvent(TEST_EVENT_NAME, "child", "builtin");
  Telemetry.recordEvent(TEST_STATIC_EVENT_NAME, "main_and_content", "object1");
  Telemetry.recordEvent(TEST_EVENT_NAME, "child", "anotherone");
}

/**
 * This function waits until content events are reported into the
 * events snapshot.
 */
async function waitForContentEvents() {
  await ContentTaskUtils.waitForCondition(() => {
    const snapshot = Telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      false
    );
    return Object.keys(snapshot).includes("content");
  });
}

add_task(async function test_setup() {
  if (!runningInParent) {
    TelemetryController.testSetupContent();
    run_child_test();
    do_send_remote_message(MESSAGE_CHILD_TEST_DONE);
    return;
  }

  // Setup.
  do_get_profile(true);
  await loadAddonManager(APP_ID, APP_NAME, APP_VERSION, PLATFORM_VERSION);
  finishAddonManagerStartup();
  fakeIntlReady();
  await TelemetryController.testSetup();
  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();
  // Enable recording for the test event category.

  // Register some dynamic builtin test events.
  Telemetry.registerBuiltinEvents(TEST_EVENT_NAME, {
    dynamic: {
      methods: ["dynamic", "child"],
      objects: ["builtin", "anotherone"],
    },
    dynamic_expired: {
      methods: ["check"],
      objects: ["expiry"],
      expired: true,
    },
  });
  Telemetry.setEventRecordingEnabled(TEST_STATIC_EVENT_NAME, true);
  Telemetry.setEventRecordingEnabled(TEST_EVENT_NAME, true);

  Telemetry.recordEvent(TEST_EVENT_NAME, "dynamic", "builtin");
  Telemetry.recordEvent(TEST_STATIC_EVENT_NAME, "main_and_content", "object1");
  Telemetry.recordEvent(TEST_EVENT_NAME, "dynamic", "anotherone");
  Telemetry.recordEvent(TEST_EVENT_NAME, "check", "expiry");

  // Run test in child, don't wait for it to finish: just wait for the
  // MESSAGE_CHILD_TEST_DONE.
  run_test_in_child("test_TelemetryChildEvents_buildFaster.js");
  await do_await_remote_message(MESSAGE_CHILD_TEST_DONE);

  // Once events are set by the content process, they don't immediately get
  // sent to the parent process. Wait for the Telemetry IPC Timer to trigger
  // and batch send the data back to the parent process.
  await waitForContentEvents();

  let snapshot = Telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    false
  );
  Assert.ok("parent" in snapshot, "Should have parent events in the snapshot.");
  Assert.ok(
    "content" in snapshot,
    "Should have content events in the snapshot."
  );

  // All events should now be recorded in the right order
  let expectedParent = [
    [TEST_EVENT_NAME, "dynamic", "builtin"],
    [TEST_STATIC_EVENT_NAME, "main_and_content", "object1"],
    [TEST_EVENT_NAME, "dynamic", "anotherone"],
  ];
  let expectedContent = [
    [TEST_EVENT_NAME, "child", "builtin"],
    [TEST_STATIC_EVENT_NAME, "main_and_content", "object1"],
    [TEST_EVENT_NAME, "child", "anotherone"],
  ];

  Assert.equal(
    snapshot.parent.length,
    expectedParent.length,
    "Should have recorded the right amount of events in parent."
  );
  for (let i = 0; i < expectedParent.length; ++i) {
    Assert.deepEqual(
      snapshot.parent[i].slice(1),
      expectedParent[i],
      "Should have recorded the expected event data in parent."
    );
  }

  Assert.equal(
    snapshot.content.length,
    expectedContent.length,
    "Should have recorded the right amount of events in content."
  );
  for (let i = 0; i < expectedContent.length; ++i) {
    Assert.deepEqual(
      snapshot.content[i].slice(1),
      expectedContent[i],
      "Should have recorded the expected event data in content."
    );
  }
});
