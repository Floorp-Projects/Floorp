/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

ChromeUtils.import("resource://services-common/utils.js");

/**
 * Return the path to the definitions file for the events.
 */
function getDefinitionsPath() {
  // Write the event definition to the spec file in the binary directory.
  let definitionFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  definitionFile = Services.dirsvc.get("GreD", Ci.nsIFile);
  definitionFile.append("EventArtifactDefinitions.json");
  return definitionFile.path;
}

add_task(async function test_setup() {
  do_get_profile();
});

add_task({
    // The test needs to write a file, and that fails in tests on Android.
    // We don't really need the Android coverage, so skip on Android.
    skip_if: () => AppConstants.platform == "android"
  }, async function test_invalidJSON() {
  const INVALID_JSON = "{ invalid,JSON { {1}";
  const FILE_PATH = getDefinitionsPath();

  // Write a corrupted JSON file.
  await OS.File.writeAtomic(FILE_PATH, INVALID_JSON, { encoding: "utf-8", noOverwrite: false });

  // Simulate Firefox startup. This should not throw!
  await TelemetryController.testSetup();
  await TelemetryController.testPromiseJsProbeRegistration();

  // Cleanup.
  await TelemetryController.testShutdown();
  await OS.File.remove(FILE_PATH);
});

add_task({
    // The test needs to write a file, and that fails in tests on Android.
    // We don't really need the Android coverage, so skip on Android.
    skip_if: () => AppConstants.platform == "android"
  }, async function test_dynamicBuiltin() {
  const DYNAMIC_EVENT_SPEC =  {
    "telemetry.test.builtin": {
      "test": {
        "objects": [
          "object1",
          "object2"
        ],
        "expired": false,
        "methods": [
          "test1",
          "test2"
        ],
        "extra_keys": [
          "key2",
          "key1"
        ],
        "record_on_release": false
      }
    }
  };

  Telemetry.clearEvents();

  // Let's write to the definition file to also cover the file
  // loading part.
  const FILE_PATH = getDefinitionsPath();
  await CommonUtils.writeJSON(DYNAMIC_EVENT_SPEC, FILE_PATH);

  // Start TelemetryController to trigger loading the specs.
  await TelemetryController.testReset();
  await TelemetryController.testPromiseJsProbeRegistration();

  // Record the events
  const TEST_EVENT_NAME = "telemetry.test.builtin";
  Telemetry.setEventRecordingEnabled(TEST_EVENT_NAME, true);
  Telemetry.recordEvent(TEST_EVENT_NAME, "test1", "object1");
  Telemetry.recordEvent(TEST_EVENT_NAME, "test2", "object1", null,
                        {"key1": "foo", "key2": "bar"});
  Telemetry.recordEvent(TEST_EVENT_NAME, "test2", "object2", null,
                        {"key2": "bar"});

  // Check the values we tried to store.
  const snapshot =
    Telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
  Assert.ok(("parent" in snapshot), "Should have parent events in the snapshot.");

  let expected = [
    [TEST_EVENT_NAME, "test1", "object1"],
    [TEST_EVENT_NAME, "test2", "object1", null, {key1: "foo", key2: "bar"}],
    [TEST_EVENT_NAME, "test2", "object2", null, {key2: "bar"}],
  ];
  let events = snapshot.parent;
  Assert.equal(events.length, expected.length, "Should have recorded the right amount of events.");
  for (let i = 0; i < expected.length; ++i) {
    Assert.deepEqual(events[i].slice(1), expected[i],
                     "Should have recorded the expected event data.");
  }

  // Clean up.
  await TelemetryController.testShutdown();
  await OS.File.remove(FILE_PATH);
});

add_task(async function test_dynamicBuiltinEvents() {
  Telemetry.clearEvents();
  Telemetry.canRecordExtended = true;

  const TEST_EVENT_NAME = "telemetry.test.dynamicbuiltin";

  // Register some dynamic builtin test events.
  Telemetry.registerBuiltinEvents(TEST_EVENT_NAME, {
    // Event with only required fields.
    "test1": {
      methods: ["test1"],
      objects: ["object1"],
    },
    // Event with extra_keys.
    "test2": {
      methods: ["test2", "test2b"],
      objects: ["object1", "object2"],
      extra_keys: ["key1", "key2"],
    },
  });

  // Record some events.
  Telemetry.setEventRecordingEnabled(TEST_EVENT_NAME, true);
  Telemetry.recordEvent(TEST_EVENT_NAME, "test1", "object1");
  Telemetry.recordEvent(TEST_EVENT_NAME, "test2", "object1", null,
                        {"key1": "foo", "key2": "bar"});
  Telemetry.recordEvent(TEST_EVENT_NAME, "test2b", "object2", null,
                        {"key2": "bar"});
  // Now check that the snapshot contains the expected data.
  let snapshot =
    Telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
  Assert.ok(("parent" in snapshot), "Should have parent events in the snapshot.");

  let expected = [
    [TEST_EVENT_NAME, "test1", "object1"],
    [TEST_EVENT_NAME, "test2", "object1", null, {key1: "foo", key2: "bar"}],
    [TEST_EVENT_NAME, "test2b", "object2", null, {key2: "bar"}],
  ];
  let events = snapshot.parent;
  Assert.equal(events.length, expected.length, "Should have recorded the right amount of events.");
  for (let i = 0; i < expected.length; ++i) {
    Assert.deepEqual(events[i].slice(1), expected[i],
                     "Should have recorded the expected event data.");
  }
});

add_task(async function test_dynamicBuiltinEventsDisabledByDefault() {
  Telemetry.clearEvents();
  Telemetry.canRecordExtended = true;

  const TEST_EVENT_NAME = "telemetry.test.offbydefault";

  // Register some dynamic builtin test events.
  Telemetry.registerBuiltinEvents(TEST_EVENT_NAME, {
    // Event with only required fields.
    "test1": {
      methods: ["test1"],
      objects: ["object1"],
    },
  });

  // Record some events.
  // Explicitely _don't_ enable the category
  Telemetry.recordEvent(TEST_EVENT_NAME, "test1", "object1");

  // Now check that the snapshot contains the expected data.
  let snapshot =
    Telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
  Assert.ok(!("parent" in snapshot), "Should not have parent events in the snapshot.");

  // Now enable the category and record again
  Telemetry.setEventRecordingEnabled(TEST_EVENT_NAME, true);
  Telemetry.recordEvent(TEST_EVENT_NAME, "test1", "object1");

  snapshot =
    Telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
  Assert.ok(("parent" in snapshot), "Should have parent events in the snapshot.");

  let expected = [
    [TEST_EVENT_NAME, "test1", "object1"],
  ];
  let events = snapshot.parent;
  Assert.equal(events.length, expected.length, "Should have recorded the right amount of events.");
  for (let i = 0; i < expected.length; ++i) {
    Assert.deepEqual(events[i].slice(1), expected[i],
                     "Should have recorded the expected event data.");
  }
});
