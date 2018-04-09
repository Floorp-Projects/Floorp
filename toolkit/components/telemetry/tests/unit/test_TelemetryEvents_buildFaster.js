/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

add_task(async function test_dynamicBuiltinEvents() {
  Telemetry.clearEvents();
  Telemetry.canRecordExtended = true;

  const TEST_EVENT_NAME = "telemetry.test.dynamicbuiltin";

  // Register some dynbamic builtin test events.
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
