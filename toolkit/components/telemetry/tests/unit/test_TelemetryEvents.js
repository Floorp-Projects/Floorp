/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

ChromeUtils.defineModuleGetter(
  this,
  "TestUtils",
  "resource://testing-common/TestUtils.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const PRERELEASE_CHANNELS = Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS;
const ALL_CHANNELS = Ci.nsITelemetry.DATASET_ALL_CHANNELS;

function checkEventFormat(events) {
  Assert.ok(Array.isArray(events), "Events should be serialized to an array.");
  for (let e of events) {
    Assert.ok(Array.isArray(e), "Event should be an array.");
    Assert.greaterOrEqual(
      e.length,
      4,
      "Event should have at least 4 elements."
    );
    Assert.lessOrEqual(e.length, 6, "Event should have at most 6 elements.");

    Assert.equal(typeof e[0], "number", "Element 0 should be a number.");
    Assert.equal(typeof e[1], "string", "Element 1 should be a string.");
    Assert.equal(typeof e[2], "string", "Element 2 should be a string.");
    Assert.equal(typeof e[3], "string", "Element 3 should be a string.");

    if (e.length > 4) {
      Assert.ok(
        e[4] === null || typeof e[4] == "string",
        "Event element 4 should be null or a string."
      );
    }
    if (e.length > 5) {
      Assert.ok(
        e[5] === null || typeof e[5] == "object",
        "Event element 5 should be null or an object."
      );
    }

    let extra = e[5];
    if (extra) {
      Assert.ok(
        Object.keys(extra).every(k => typeof k == "string"),
        "All extra keys should be strings."
      );
      Assert.ok(
        Object.values(extra).every(v => typeof v == "string"),
        "All extra values should be strings."
      );
    }
  }
}

/**
 * @param summaries is of the form
 *   [{process, [event category, event object, event method], count}]
 * @param clearScalars - true if you want to clear the scalars
 */
function checkEventSummary(summaries, clearScalars) {
  let scalars = Telemetry.getSnapshotForKeyedScalars("main", clearScalars);

  for (let [process, [category, eObject, method], count] of summaries) {
    let uniqueEventName = `${category}#${eObject}#${method}`;
    let summaryCount;
    if (process === "dynamic") {
      summaryCount =
        scalars.dynamic["telemetry.dynamic_event_counts"][uniqueEventName];
    } else {
      summaryCount =
        scalars[process]["telemetry.event_counts"][uniqueEventName];
    }
    Assert.equal(
      summaryCount,
      count,
      `${uniqueEventName} had wrong summary count`
    );
  }
}

function checkRegistrationFailure(failureType) {
  let snapshot = Telemetry.getSnapshotForHistograms("main", true);
  Assert.ok(
    "parent" in snapshot,
    "There should be at least one parent histogram when checking for registration failures."
  );
  Assert.ok(
    "TELEMETRY_EVENT_REGISTRATION_ERROR" in snapshot.parent,
    "TELEMETRY_EVENT_REGISTRATION_ERROR should exist when checking for registration failures."
  );
  let values = snapshot.parent.TELEMETRY_EVENT_REGISTRATION_ERROR.values;
  Assert.ok(
    !!values,
    "TELEMETRY_EVENT_REGISTRATION_ERROR's values should exist when checking for registration failures."
  );
  Assert.equal(
    values[failureType],
    1,
    `Event registration ought to have failed due to type ${failureType}`
  );
}

function checkRecordingFailure(failureType) {
  let snapshot = Telemetry.getSnapshotForHistograms("main", true);
  Assert.ok(
    "parent" in snapshot,
    "There should be at least one parent histogram when checking for recording failures."
  );
  Assert.ok(
    "TELEMETRY_EVENT_RECORDING_ERROR" in snapshot.parent,
    "TELEMETRY_EVENT_RECORDING_ERROR should exist when checking for recording failures."
  );
  let values = snapshot.parent.TELEMETRY_EVENT_RECORDING_ERROR.values;
  Assert.ok(
    !!values,
    "TELEMETRY_EVENT_RECORDING_ERROR's values should exist when checking for recording failures."
  );
  Assert.equal(
    values[failureType],
    1,
    `Event recording ought to have failed due to type ${failureType}`
  );
}

add_task(async function test_event_summary_limit() {
  Telemetry.clearEvents();
  Telemetry.clearScalars();

  const limit = 50;
  Services.prefs.setIntPref("toolkit.telemetry.maxEventSummaryKeys", limit);
  let objects = [];
  for (let i = 0; i < limit + 1; i++) {
    objects.push("object" + i);
  }
  // Using "telemetry.test.dynamic" as using "telemetry.test" will enable
  // the "telemetry.test" category.
  Telemetry.registerEvents("telemetry.test.dynamic", {
    test_method: {
      methods: ["testMethod"],
      objects,
      record_on_release: true,
    },
  });
  for (let object of objects) {
    Telemetry.recordEvent("telemetry.test.dynamic", "testMethod", object);
  }

  TelemetryTestUtils.assertNumberOfEvents(
    limit + 1,
    {},
    { process: "dynamic" }
  );
  let scalarSnapshot = Telemetry.getSnapshotForKeyedScalars("main", true);
  Assert.equal(
    Object.keys(scalarSnapshot.dynamic["telemetry.dynamic_event_counts"])
      .length,
    limit,
    "Should not have recorded more than `limit` events"
  );
});

add_task(async function test_recording_state() {
  Telemetry.clearEvents();
  Telemetry.clearScalars();

  const events = [
    ["telemetry.test", "test1", "object1"],
    ["telemetry.test.second", "test", "object1"],
  ];

  // Both test categories should be off by default.
  events.forEach(e => Telemetry.recordEvent(...e));
  TelemetryTestUtils.assertEvents([]);
  checkEventSummary(
    events.map(e => ["parent", e, 1]),
    true
  );

  // Enable one test category and see that we record correctly.
  Telemetry.setEventRecordingEnabled("telemetry.test", true);
  events.forEach(e => Telemetry.recordEvent(...e));
  TelemetryTestUtils.assertEvents([events[0]]);
  checkEventSummary(
    events.map(e => ["parent", e, 1]),
    true
  );

  // Also enable the other test category and see that we record correctly.
  Telemetry.setEventRecordingEnabled("telemetry.test.second", true);
  events.forEach(e => Telemetry.recordEvent(...e));
  TelemetryTestUtils.assertEvents(events);
  checkEventSummary(
    events.map(e => ["parent", e, 1]),
    true
  );

  // Now turn of one category again and check that this works as expected.
  Telemetry.setEventRecordingEnabled("telemetry.test", false);
  events.forEach(e => Telemetry.recordEvent(...e));
  TelemetryTestUtils.assertEvents([events[1]]);
  checkEventSummary(
    events.map(e => ["parent", e, 1]),
    true
  );
});

add_task(async function recording_setup() {
  // Make sure both test categories are enabled for the remaining tests.
  // Otherwise their event recording won't work.
  Telemetry.setEventRecordingEnabled("telemetry.test", true);
  Telemetry.setEventRecordingEnabled("telemetry.test.second", true);
});

add_task(async function test_recording() {
  Telemetry.clearScalars();
  Telemetry.clearEvents();

  // Record some events.
  let expected = [
    { optout: false, event: ["telemetry.test", "test1", "object1"] },
    { optout: false, event: ["telemetry.test", "test2", "object2"] },

    { optout: false, event: ["telemetry.test", "test1", "object1", "value"] },
    {
      optout: false,
      event: ["telemetry.test", "test1", "object1", "value", null],
    },
    {
      optout: false,
      event: ["telemetry.test", "test1", "object1", null, { key1: "value1" }],
    },
    {
      optout: false,
      event: [
        "telemetry.test",
        "test1",
        "object1",
        "value",
        { key1: "value1", key2: "value2" },
      ],
    },

    { optout: true, event: ["telemetry.test", "optout", "object1"] },
    { optout: false, event: ["telemetry.test.second", "test", "object1"] },
    {
      optout: false,
      event: [
        "telemetry.test.second",
        "test",
        "object1",
        null,
        { key1: "value1" },
      ],
    },
  ];

  for (let entry of expected) {
    entry.tsBefore = Math.floor(Telemetry.msSinceProcessStart());
    try {
      Telemetry.recordEvent(...entry.event);
    } catch (ex) {
      Assert.ok(
        false,
        `Failed to record event ${JSON.stringify(entry.event)}: ${ex}`
      );
    }
    entry.tsAfter = Math.floor(Telemetry.msSinceProcessStart());
  }

  // Strip off trailing null values to match the serialized events.
  for (let entry of expected) {
    let e = entry.event;
    while (e.length >= 3 && e[e.length - 1] === null) {
      e.pop();
    }
  }

  // Check that the events were summarized properly.
  let summaries = {};
  expected.forEach(({ optout, event }) => {
    let [category, eObject, method] = event;
    let uniqueEventName = `${category}#${eObject}#${method}`;
    if (!(uniqueEventName in summaries)) {
      summaries[uniqueEventName] = ["parent", event, 1];
    } else {
      summaries[uniqueEventName][2]++;
    }
  });
  checkEventSummary(Object.values(summaries), true);

  // The following should not result in any recorded events.
  Telemetry.recordEvent("unknown.category", "test1", "object1");
  checkRecordingFailure(0 /* UnknownEvent */);
  Telemetry.recordEvent("telemetry.test", "unknown", "object1");
  checkRecordingFailure(0 /* UnknownEvent */);
  Telemetry.recordEvent("telemetry.test", "test1", "unknown");
  checkRecordingFailure(0 /* UnknownEvent */);

  let checkEvents = (events, expectedEvents) => {
    checkEventFormat(events);
    Assert.equal(
      events.length,
      expectedEvents.length,
      "Snapshot should have the right number of events."
    );

    for (let i = 0; i < events.length; ++i) {
      let { tsBefore, tsAfter } = expectedEvents[i];
      let ts = events[i][0];
      Assert.greaterOrEqual(
        ts,
        tsBefore,
        "The recorded timestamp should be greater than the one before recording."
      );
      Assert.lessOrEqual(
        ts,
        tsAfter,
        "The recorded timestamp should be less than the one after recording."
      );

      let recordedData = events[i].slice(1);
      let expectedData = expectedEvents[i].event.slice();
      Assert.deepEqual(
        recordedData,
        expectedData,
        "The recorded event data should match."
      );
    }
  };

  // Check that the expected events were recorded.
  let snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, false);
  Assert.ok("parent" in snapshot, "Should have entry for main process.");
  checkEvents(snapshot.parent, expected);

  // Check serializing only opt-out events.
  snapshot = Telemetry.snapshotEvents(ALL_CHANNELS, false);
  Assert.ok("parent" in snapshot, "Should have entry for main process.");
  let filtered = expected.filter(e => !!e.optout);
  checkEvents(snapshot.parent, filtered);
});

add_task(async function test_clear() {
  Telemetry.clearEvents();

  const COUNT = 10;
  for (let i = 0; i < COUNT; ++i) {
    Telemetry.recordEvent("telemetry.test", "test1", "object1");
    Telemetry.recordEvent("telemetry.test.second", "test", "object1");
  }

  // Check that events were recorded.
  // The events are cleared by passing the respective flag.
  let snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, true);
  Assert.ok("parent" in snapshot, "Should have entry for main process.");
  Assert.equal(
    snapshot.parent.length,
    2 * COUNT,
    `Should have recorded ${2 * COUNT} events.`
  );

  // Now the events should be cleared.
  snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, false);
  Assert.equal(
    Object.keys(snapshot).length,
    0,
    `Should have cleared the events.`
  );

  for (let i = 0; i < COUNT; ++i) {
    Telemetry.recordEvent("telemetry.test", "test1", "object1");
    Telemetry.recordEvent("telemetry.test.second", "test", "object1");
  }
  snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, true, 5);
  Assert.ok("parent" in snapshot, "Should have entry for main process.");
  Assert.equal(snapshot.parent.length, 5, "Should have returned 5 events");
  snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, false);
  Assert.ok("parent" in snapshot, "Should have entry for main process.");
  Assert.equal(
    snapshot.parent.length,
    2 * COUNT - 5,
    `Should have returned ${2 * COUNT - 5} events`
  );

  Telemetry.recordEvent("telemetry.test", "test1", "object1");
  snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, false, 5);
  Assert.ok("parent" in snapshot, "Should have entry for main process.");
  Assert.equal(snapshot.parent.length, 5, "Should have returned 5 events");
  snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, true);
  Assert.ok("parent" in snapshot, "Should have entry for main process.");
  Assert.equal(
    snapshot.parent.length,
    2 * COUNT - 5 + 1,
    `Should have returned ${2 * COUNT - 5 + 1} events`
  );
});

add_task(async function test_expiry() {
  Telemetry.clearEvents();

  // Recording call with event that is expired by version.
  Telemetry.recordEvent("telemetry.test", "expired_version", "object1");
  checkRecordingFailure(1 /* Expired */);
  let snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, true);
  Assert.equal(
    Object.keys(snapshot).length,
    0,
    "Should not record event with expired version."
  );

  // Recording call with event that has expiry_version set into the future.
  Telemetry.recordEvent("telemetry.test", "not_expired_optout", "object1");
  TelemetryTestUtils.assertNumberOfEvents(1);
});

add_task(async function test_invalidParams() {
  Telemetry.clearEvents();

  // Recording call with wrong type for value argument.
  Telemetry.recordEvent("telemetry.test", "test1", "object1", 1);
  let snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, true);
  Assert.equal(
    Object.keys(snapshot).length,
    0,
    "Should not record event when value argument with invalid type is passed."
  );
  checkRecordingFailure(3 /* Value */);

  // Recording call with wrong type for extra argument.
  Telemetry.recordEvent("telemetry.test", "test1", "object1", null, "invalid");
  snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, true);
  Assert.equal(
    Object.keys(snapshot).length,
    0,
    "Should not record event when extra argument with invalid type is passed."
  );
  checkRecordingFailure(4 /* Extra */);

  // Recording call with unknown extra key.
  Telemetry.recordEvent("telemetry.test", "test1", "object1", null, {
    key3: "x",
  });
  snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, true);
  Assert.equal(
    Object.keys(snapshot).length,
    0,
    "Should not record event when extra argument with invalid key is passed."
  );
  checkRecordingFailure(2 /* ExtraKey */);

  // Recording call with invalid value type.
  Telemetry.recordEvent("telemetry.test", "test1", "object1", null, {
    key3: 1,
  });
  snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, true);
  Assert.equal(
    Object.keys(snapshot).length,
    0,
    "Should not record event when extra argument with invalid value type is passed."
  );
  checkRecordingFailure(4 /* Extra */);
});

add_task(async function test_storageLimit() {
  Telemetry.clearEvents();

  let limitReached = TestUtils.topicObserved(
    "event-telemetry-storage-limit-reached"
  );
  // Record more events than the storage limit allows.
  let LIMIT = 1000;
  let COUNT = LIMIT + 10;
  for (let i = 0; i < COUNT; ++i) {
    Telemetry.recordEvent("telemetry.test", "test1", "object1", String(i));
  }

  await limitReached;
  Assert.ok(true, "Topic was notified when event limit was reached");

  // Check that the right events were recorded.
  let snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, true);
  Assert.ok("parent" in snapshot, "Should have entry for main process.");
  let events = snapshot.parent;
  Assert.equal(
    events.length,
    COUNT,
    `Should have only recorded all ${COUNT} events`
  );
  Assert.ok(
    events.every((e, idx) => e[4] === String(idx)),
    "Should have recorded all events."
  );
});

add_task(async function test_valueLimits() {
  Telemetry.clearEvents();

  // Record values that are at or over the limits for string lengths.
  let LIMIT = 80;
  let expected = [
    ["telemetry.test", "test1", "object1", "a".repeat(LIMIT - 10), null],
    ["telemetry.test", "test1", "object1", "a".repeat(LIMIT), null],
    ["telemetry.test", "test1", "object1", "a".repeat(LIMIT + 1), null],
    ["telemetry.test", "test1", "object1", "a".repeat(LIMIT + 10), null],

    [
      "telemetry.test",
      "test1",
      "object1",
      null,
      { key1: "a".repeat(LIMIT - 10) },
    ],
    ["telemetry.test", "test1", "object1", null, { key1: "a".repeat(LIMIT) }],
    [
      "telemetry.test",
      "test1",
      "object1",
      null,
      { key1: "a".repeat(LIMIT + 1) },
    ],
    [
      "telemetry.test",
      "test1",
      "object1",
      null,
      { key1: "a".repeat(LIMIT + 10) },
    ],
  ];

  for (let event of expected) {
    Telemetry.recordEvent(...event);
    if (event[3]) {
      event[3] = event[3].substr(0, LIMIT);
    } else {
      event[3] = undefined;
    }
    if (event[4]) {
      event[4].key1 = event[4].key1.substr(0, LIMIT);
    }
  }

  // Strip off trailing null values to match the serialized events.
  for (let e of expected) {
    while (e.length >= 3 && e[e.length - 1] === null) {
      e.pop();
    }
  }

  // Check that the right events were recorded.
  TelemetryTestUtils.assertEvents(expected);
});

add_task(async function test_unicodeValues() {
  Telemetry.clearEvents();

  // Record string values containing unicode characters.
  let value = "漢語";
  Telemetry.recordEvent("telemetry.test", "test1", "object1", value);
  Telemetry.recordEvent("telemetry.test", "test1", "object1", null, {
    key1: value,
  });

  // Check that the values were correctly recorded.
  TelemetryTestUtils.assertEvents([{ value }, { extra: { key1: value } }]);
});

add_task(async function test_dynamicEvents() {
  Telemetry.clearEvents();
  Telemetry.clearScalars();
  Telemetry.canRecordExtended = true;

  // Register some test events.
  Telemetry.registerEvents("telemetry.test.dynamic", {
    // Event with only required fields.
    test1: {
      methods: ["test1"],
      objects: ["object1"],
    },
    // Event with extra_keys.
    test2: {
      methods: ["test2", "test2b"],
      objects: ["object1"],
      extra_keys: ["key1", "key2"],
    },
    // Expired event.
    test3: {
      methods: ["test3"],
      objects: ["object1"],
      expired: true,
    },
    // A release-channel recording event.
    test4: {
      methods: ["test4"],
      objects: ["object1"],
      record_on_release: true,
    },
  });

  // Record some valid events.
  Telemetry.recordEvent("telemetry.test.dynamic", "test1", "object1");
  Telemetry.recordEvent("telemetry.test.dynamic", "test2", "object1", null, {
    key1: "foo",
    key2: "bar",
  });
  Telemetry.recordEvent("telemetry.test.dynamic", "test2b", "object1", null, {
    key1: "foo",
    key2: "bar",
  });
  Telemetry.recordEvent(
    "telemetry.test.dynamic",
    "test3",
    "object1",
    "some value"
  );
  Telemetry.recordEvent("telemetry.test.dynamic", "test4", "object1", null);

  // Test recording an unknown event.
  Telemetry.recordEvent("telemetry.test.dynamic", "unknown", "unknown");
  checkRecordingFailure(0 /* UnknownEvent */);

  // Now check that the snapshot contains the expected data.
  let snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, false);
  Assert.ok(
    "dynamic" in snapshot,
    "Should have dynamic events in the snapshot."
  );

  let expected = [
    ["telemetry.test.dynamic", "test1", "object1"],
    [
      "telemetry.test.dynamic",
      "test2",
      "object1",
      null,
      { key1: "foo", key2: "bar" },
    ],
    [
      "telemetry.test.dynamic",
      "test2b",
      "object1",
      null,
      { key1: "foo", key2: "bar" },
    ],
    // "test3" is epxired, so it should not be recorded.
    ["telemetry.test.dynamic", "test4", "object1"],
  ];
  let events = snapshot.dynamic;
  Assert.equal(
    events.length,
    expected.length,
    "Should have recorded the right amount of events."
  );
  for (let i = 0; i < expected.length; ++i) {
    Assert.deepEqual(
      events[i].slice(1),
      expected[i],
      "Should have recorded the expected event data."
    );
  }

  // Check that we've summarized the recorded events
  checkEventSummary(
    expected.map(ev => ["dynamic", ev, 1]),
    true
  );

  // Check that the opt-out snapshot contains only the one expected event.
  snapshot = Telemetry.snapshotEvents(ALL_CHANNELS, false);
  Assert.ok(
    "dynamic" in snapshot,
    "Should have dynamic events in the snapshot."
  );
  Assert.equal(
    snapshot.dynamic.length,
    1,
    "Should have one opt-out event in the snapshot."
  );
  expected = ["telemetry.test.dynamic", "test4", "object1"];
  Assert.deepEqual(snapshot.dynamic[0].slice(1), expected);

  // Recording with unknown extra keys should be ignored and print an error.
  Telemetry.clearEvents();
  Telemetry.recordEvent("telemetry.test.dynamic", "test1", "object1", null, {
    key1: "foo",
  });
  Telemetry.recordEvent("telemetry.test.dynamic", "test2", "object1", null, {
    key1: "foo",
    unknown: "bar",
  });
  snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, true);
  Assert.ok(
    !("dynamic" in snapshot),
    "Should have not recorded dynamic events with unknown extra keys."
  );

  // Other built-in events should not show up in the "dynamic" bucket of the snapshot.
  Telemetry.recordEvent("telemetry.test", "test1", "object1");
  snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, true);
  Assert.ok(
    !("dynamic" in snapshot),
    "Should have not recorded built-in event into dynamic bucket."
  );

  // Test that recording opt-in and opt-out events works as expected.
  Telemetry.clearEvents();
  Telemetry.canRecordExtended = false;

  Telemetry.recordEvent("telemetry.test.dynamic", "test1", "object1");
  Telemetry.recordEvent("telemetry.test.dynamic", "test4", "object1");

  expected = [
    // Only "test4" should have been recorded.
    ["telemetry.test.dynamic", "test4", "object1"],
  ];
  snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, true);
  Assert.equal(
    snapshot.dynamic.length,
    1,
    "Should have one opt-out event in the snapshot."
  );
  Assert.deepEqual(
    snapshot.dynamic.map(e => e.slice(1)),
    expected
  );
});

add_task(async function test_dynamicEventRegistrationValidation() {
  Telemetry.canRecordExtended = true;
  Telemetry.clearEvents();

  // Test registration of invalid categories.
  Telemetry.getSnapshotForHistograms("main", true); // Clear histograms before we begin.
  Assert.throws(
    () =>
      Telemetry.registerEvents("telemetry+test+dynamic", {
        test1: {
          methods: ["test1"],
          objects: ["object1"],
        },
      }),
    /Category parameter should match the identifier pattern\./,
    "Should throw when registering category names with invalid characters."
  );
  checkRegistrationFailure(2 /* Category */);
  Assert.throws(
    () =>
      Telemetry.registerEvents(
        "telemetry.test.test.test.test.test.test.test.test",
        {
          test1: {
            methods: ["test1"],
            objects: ["object1"],
          },
        }
      ),
    /Category parameter should match the identifier pattern\./,
    "Should throw when registering overly long category names."
  );
  checkRegistrationFailure(2 /* Category */);

  // Test registration of invalid event names.
  Assert.throws(
    () =>
      Telemetry.registerEvents("telemetry.test.dynamic1", {
        "test?1": {
          methods: ["test1"],
          objects: ["object1"],
        },
      }),
    /Event names should match the identifier pattern\./,
    "Should throw when registering event names with invalid characters."
  );
  checkRegistrationFailure(1 /* Name */);
  Assert.throws(
    () =>
      Telemetry.registerEvents("telemetry.test.dynamic2", {
        test1test1test1test1test1test1test1: {
          methods: ["test1"],
          objects: ["object1"],
        },
      }),
    /Event names should match the identifier pattern\./,
    "Should throw when registering overly long event names."
  );
  checkRegistrationFailure(1 /* Name */);

  // Test registration of invalid method names.
  Assert.throws(
    () =>
      Telemetry.registerEvents("telemetry.test.dynamic3", {
        test1: {
          methods: ["test?1"],
          objects: ["object1"],
        },
      }),
    /Method names should match the identifier pattern\./,
    "Should throw when registering method names with invalid characters."
  );
  checkRegistrationFailure(3 /* Method */);
  Assert.throws(
    () =>
      Telemetry.registerEvents("telemetry.test.dynamic", {
        test1: {
          methods: ["test1test1test1test1test1test1test1"],
          objects: ["object1"],
        },
      }),
    /Method names should match the identifier pattern\./,
    "Should throw when registering overly long method names."
  );
  checkRegistrationFailure(3 /* Method */);

  // Test registration of invalid object names.
  Assert.throws(
    () =>
      Telemetry.registerEvents("telemetry.test.dynamic4", {
        test1: {
          methods: ["test1"],
          objects: ["object?1"],
        },
      }),
    /Object names should match the identifier pattern\./,
    "Should throw when registering object names with invalid characters."
  );
  checkRegistrationFailure(4 /* Object */);
  Assert.throws(
    () =>
      Telemetry.registerEvents("telemetry.test.dynamic5", {
        test1: {
          methods: ["test1"],
          objects: ["object1object1object1object1object1object1"],
        },
      }),
    /Object names should match the identifier pattern\./,
    "Should throw when registering overly long object names."
  );
  checkRegistrationFailure(4 /* Object */);

  // Test validation of invalid key names.
  Assert.throws(
    () =>
      Telemetry.registerEvents("telemetry.test.dynamic6", {
        test1: {
          methods: ["test1"],
          objects: ["object1"],
          extra_keys: ["a?1"],
        },
      }),
    /Extra key names should match the identifier pattern\./,
    "Should throw when registering extra key names with invalid characters."
  );
  checkRegistrationFailure(5 /* ExtraKeys */);

  // Test validation of key names that are too long - we allow a maximum of 15 characters.
  Assert.throws(
    () =>
      Telemetry.registerEvents("telemetry.test.dynamic7", {
        test1: {
          methods: ["test1"],
          objects: ["object1"],
          extra_keys: ["a012345678901234"],
        },
      }),
    /Extra key names should match the identifier pattern\./,
    "Should throw when registering extra key names which are too long."
  );
  checkRegistrationFailure(5 /* ExtraKeys */);
  Telemetry.registerEvents("telemetry.test.dynamic8", {
    test1: {
      methods: ["test1"],
      objects: ["object1"],
      extra_keys: ["a01234567890123"],
    },
  });

  // Test validation of extra key count - we only allow 10.
  Assert.throws(
    () =>
      Telemetry.registerEvents("telemetry.test.dynamic9", {
        test1: {
          methods: ["test1"],
          objects: ["object1"],
          extra_keys: [
            "a1",
            "a2",
            "a3",
            "a4",
            "a5",
            "a6",
            "a7",
            "a8",
            "a9",
            "a10",
            "a11",
          ],
        },
      }),
    /No more than 10 extra keys can be registered\./,
    "Should throw when registering too many extra keys."
  );
  checkRegistrationFailure(5 /* ExtraKeys */);
  Telemetry.registerEvents("telemetry.test.dynamic10", {
    test1: {
      methods: ["test1"],
      objects: ["object1"],
      extra_keys: ["a1", "a2", "a3", "a4", "a5", "a6", "a7", "a8", "a9", "a10"],
    },
  });
});

// When add-ons update, they may re-register some of the dynamic events.
// Test through some possible scenarios.
add_task(async function test_dynamicEventRegisterAgain() {
  Telemetry.canRecordExtended = true;
  Telemetry.clearEvents();

  const category = "telemetry.test.register.again";
  let events = {
    test1: {
      methods: ["test1"],
      objects: ["object1"],
    },
  };

  // First register the initial event and make sure it can be recorded.
  Telemetry.registerEvents(category, events);
  let expected = [[category, "test1", "object1"]];
  expected.forEach(e => Telemetry.recordEvent(...e));

  let snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, true);
  Assert.equal(
    snapshot.dynamic.length,
    expected.length,
    "Should have right number of events in the snapshot."
  );
  Assert.deepEqual(
    snapshot.dynamic.map(e => e.slice(1)),
    expected
  );

  // Register the same event again and make sure it can still be recorded.
  Telemetry.registerEvents(category, events);
  Telemetry.recordEvent(category, "test1", "object1");

  snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, true);
  Assert.equal(
    snapshot.dynamic.length,
    expected.length,
    "Should have right number of events in the snapshot."
  );
  Assert.deepEqual(
    snapshot.dynamic.map(e => e.slice(1)),
    expected
  );

  // Now register another event in the same category and make sure both events can be recorded.
  events.test2 = {
    methods: ["test2"],
    objects: ["object2"],
  };
  Telemetry.registerEvents(category, events);

  expected = [
    [category, "test1", "object1"],
    [category, "test2", "object2"],
  ];
  expected.forEach(e => Telemetry.recordEvent(...e));

  snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, true);
  Assert.equal(
    snapshot.dynamic.length,
    expected.length,
    "Should have right number of events in the snapshot."
  );
  Assert.deepEqual(
    snapshot.dynamic.map(e => e.slice(1)),
    expected
  );

  // Check that adding a new object to an event entry works.
  events.test1.methods = ["test1a"];
  events.test2.objects = ["object2", "object2a"];
  Telemetry.registerEvents(category, events);

  expected = [
    [category, "test1", "object1"],
    [category, "test2", "object2"],
    [category, "test1a", "object1"],
    [category, "test2", "object2a"],
  ];
  expected.forEach(e => Telemetry.recordEvent(...e));

  snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, true);
  Assert.equal(
    snapshot.dynamic.length,
    expected.length,
    "Should have right number of events in the snapshot."
  );
  Assert.deepEqual(
    snapshot.dynamic.map(e => e.slice(1)),
    expected
  );

  // Make sure that we can expire events that are already registered.
  events.test2.expired = true;
  Telemetry.registerEvents(category, events);

  expected = [[category, "test1", "object1"]];
  expected.forEach(e => Telemetry.recordEvent(...e));

  snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, true);
  Assert.equal(
    snapshot.dynamic.length,
    expected.length,
    "Should have right number of events in the snapshot."
  );
  Assert.deepEqual(
    snapshot.dynamic.map(e => e.slice(1)),
    expected
  );
});

add_task(
  {
    skip_if: () => gIsAndroid,
  },
  async function test_productSpecificEvents() {
    const EVENT_CATEGORY = "telemetry.test";
    const DEFAULT_PRODUCTS_EVENT = "default_products";
    const DESKTOP_ONLY_EVENT = "desktop_only";
    const MULTIPRODUCT_EVENT = "multiproduct";
    const MOBILE_ONLY_EVENT = "mobile_only";

    Telemetry.clearEvents();

    // Try to record the desktop and multiproduct event
    Telemetry.recordEvent(EVENT_CATEGORY, DEFAULT_PRODUCTS_EVENT, "object1");
    Telemetry.recordEvent(EVENT_CATEGORY, DESKTOP_ONLY_EVENT, "object1");
    Telemetry.recordEvent(EVENT_CATEGORY, MULTIPRODUCT_EVENT, "object1");

    // Try to record the mobile-only event
    Telemetry.recordEvent(EVENT_CATEGORY, MOBILE_ONLY_EVENT, "object1");

    let events = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, true).parent;

    let expected = [
      [EVENT_CATEGORY, DEFAULT_PRODUCTS_EVENT, "object1"],
      [EVENT_CATEGORY, DESKTOP_ONLY_EVENT, "object1"],
      [EVENT_CATEGORY, MULTIPRODUCT_EVENT, "object1"],
    ];
    Assert.equal(
      events.length,
      expected.length,
      "Should have recorded the right amount of events."
    );
    for (let i = 0; i < expected.length; ++i) {
      Assert.deepEqual(
        events[i].slice(1),
        expected[i],
        "Should have recorded the expected event data."
      );
    }
  }
);

add_task(
  {
    skip_if: () => !gIsAndroid,
  },
  async function test_mobileSpecificEvents() {
    const EVENT_CATEGORY = "telemetry.test";
    const DEFAULT_PRODUCTS_EVENT = "default_products";
    const DESKTOP_ONLY_EVENT = "desktop_only";
    const MULTIPRODUCT_EVENT = "multiproduct";
    const MOBILE_ONLY_EVENT = "mobile_only";

    Telemetry.clearEvents();

    // Try to record the mobile-only and multiproduct event
    Telemetry.recordEvent(EVENT_CATEGORY, DEFAULT_PRODUCTS_EVENT, "object1");
    Telemetry.recordEvent(EVENT_CATEGORY, MOBILE_ONLY_EVENT, "object1");
    Telemetry.recordEvent(EVENT_CATEGORY, MULTIPRODUCT_EVENT, "object1");

    // Try to record the mobile-only event
    Telemetry.recordEvent(EVENT_CATEGORY, DESKTOP_ONLY_EVENT, "object1");

    let events = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, true).parent;

    let expected = [
      [EVENT_CATEGORY, DEFAULT_PRODUCTS_EVENT, "object1"],
      [EVENT_CATEGORY, MOBILE_ONLY_EVENT, "object1"],
      [EVENT_CATEGORY, MULTIPRODUCT_EVENT, "object1"],
    ];
    Assert.equal(
      events.length,
      expected.length,
      "Should have recorded the right amount of events."
    );
    for (let i = 0; i < expected.length; ++i) {
      Assert.deepEqual(
        events[i].slice(1),
        expected[i],
        "Should have recorded the expected event data."
      );
    }
  }
);
