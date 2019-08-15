/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

const { CommonUtils } = ChromeUtils.import(
  "resource://services-common/utils.js"
);

/**
 * Return the path to the definitions file for the events.
 */
function getDefinitionsPath() {
  // Write the event definition to the spec file in the binary directory.
  let definitionFile = Cc["@mozilla.org/file/local;1"].createInstance(
    Ci.nsIFile
  );
  definitionFile = Services.dirsvc.get("GreD", Ci.nsIFile);
  definitionFile.append("EventArtifactDefinitions.json");
  return definitionFile.path;
}

add_task(async function test_setup() {
  do_get_profile();
});

add_task(
  {
    // The test needs to write a file, and that fails in tests on Android.
    // We don't really need the Android coverage, so skip on Android.
    skip_if: () => AppConstants.platform == "android",
  },
  async function test_invalidJSON() {
    const INVALID_JSON = "{ invalid,JSON { {1}";
    const FILE_PATH = getDefinitionsPath();

    // Write a corrupted JSON file.
    await OS.File.writeAtomic(FILE_PATH, INVALID_JSON, {
      encoding: "utf-8",
      noOverwrite: false,
    });

    // Simulate Firefox startup. This should not throw!
    await TelemetryController.testSetup();
    await TelemetryController.testPromiseJsProbeRegistration();

    // Cleanup.
    await TelemetryController.testShutdown();
    await OS.File.remove(FILE_PATH);
  }
);

add_task(
  {
    // The test needs to write a file, and that fails in tests on Android.
    // We don't really need the Android coverage, so skip on Android.
    skip_if: () => AppConstants.platform == "android",
  },
  async function test_dynamicBuiltin() {
    const DYNAMIC_EVENT_SPEC = {
      "telemetry.test.builtin": {
        test: {
          objects: ["object1", "object2"],
          expires: "never",
          methods: ["test1", "test2"],
          extra_keys: ["key2", "key1"],
          record_on_release: false,
        },
      },
      // Test a new, expired event
      "telemetry.test.expired": {
        expired: {
          objects: ["object1"],
          methods: ["method1"],
          expires: AppConstants.MOZ_APP_VERSION,
          record_on_release: false,
        },
      },
      // Test overwriting static expiries
      "telemetry.test": {
        expired_version: {
          objects: ["object1"],
          methods: ["expired_version"],
          expires: "never",
          record_on_release: false,
        },
        not_expired_optout: {
          objects: ["object1"],
          methods: ["not_expired_optout"],
          expires: AppConstants.MOZ_APP_VERSION,
          record_on_release: true,
        },
      },
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
    const DYNAMIC_EVENT_CATEGORY = "telemetry.test.expired";
    const STATIC_EVENT_CATEGORY = "telemetry.test";
    Telemetry.setEventRecordingEnabled(TEST_EVENT_NAME, true);
    Telemetry.setEventRecordingEnabled(DYNAMIC_EVENT_CATEGORY, true);
    Telemetry.setEventRecordingEnabled(STATIC_EVENT_CATEGORY, true);
    Telemetry.recordEvent(TEST_EVENT_NAME, "test1", "object1");
    Telemetry.recordEvent(TEST_EVENT_NAME, "test2", "object1", null, {
      key1: "foo",
      key2: "bar",
    });
    Telemetry.recordEvent(TEST_EVENT_NAME, "test2", "object2", null, {
      key2: "bar",
    });
    Telemetry.recordEvent(DYNAMIC_EVENT_CATEGORY, "method1", "object1");
    Telemetry.recordEvent(STATIC_EVENT_CATEGORY, "expired_version", "object1");
    Telemetry.recordEvent(
      STATIC_EVENT_CATEGORY,
      "not_expired_optout",
      "object1"
    );

    // Check the values we tried to store.
    const snapshot = Telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      false
    );
    Assert.ok(
      "parent" in snapshot,
      "Should have parent events in the snapshot."
    );

    let expected = [
      [TEST_EVENT_NAME, "test1", "object1"],
      [TEST_EVENT_NAME, "test2", "object1", null, { key1: "foo", key2: "bar" }],
      [TEST_EVENT_NAME, "test2", "object2", null, { key2: "bar" }],
      [STATIC_EVENT_CATEGORY, "expired_version", "object1"],
    ];
    let events = snapshot.parent;
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

    // Clean up.
    await TelemetryController.testShutdown();
    await OS.File.remove(FILE_PATH);
  }
);

add_task(async function test_dynamicBuiltinEvents() {
  Telemetry.clearEvents();
  Telemetry.clearScalars();
  Telemetry.canRecordExtended = true;

  const TEST_EVENT_NAME = "telemetry.test.dynamicbuiltin";

  // Register some dynamic builtin test events.
  Telemetry.registerBuiltinEvents(TEST_EVENT_NAME, {
    // Event with only required fields.
    test1: {
      methods: ["test1"],
      objects: ["object1"],
    },
    // Event with extra_keys.
    test2: {
      methods: ["test2", "test2b"],
      objects: ["object1", "object2"],
      extra_keys: ["key1", "key2"],
    },
  });

  // Record some events.
  Telemetry.setEventRecordingEnabled(TEST_EVENT_NAME, true);
  Telemetry.recordEvent(TEST_EVENT_NAME, "test1", "object1");
  Telemetry.recordEvent(TEST_EVENT_NAME, "test2", "object1", null, {
    key1: "foo",
    key2: "bar",
  });
  Telemetry.recordEvent(TEST_EVENT_NAME, "test2b", "object2", null, {
    key2: "bar",
  });
  // Now check that the snapshot contains the expected data.
  let snapshot = Telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    false
  );
  Assert.ok("parent" in snapshot, "Should have parent events in the snapshot.");

  // For checking event summaries
  const scalars = Telemetry.getSnapshotForKeyedScalars("main", true);
  Assert.ok(
    "parent" in scalars,
    "Should have parent scalars in the main snapshot."
  );

  let expected = [
    [TEST_EVENT_NAME, "test1", "object1"],
    [TEST_EVENT_NAME, "test2", "object1", null, { key1: "foo", key2: "bar" }],
    [TEST_EVENT_NAME, "test2b", "object2", null, { key2: "bar" }],
  ];
  let events = snapshot.parent;
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

    const uniqueEventName = `${expected[i][0]}#${expected[i][1]}#${
      expected[i][2]
    }`;
    const summaryCount =
      scalars.parent["telemetry.event_counts"][uniqueEventName];
    Assert.equal(1, summaryCount, `${uniqueEventName} had wrong summary count`);
  }
});

add_task(async function test_dynamicBuiltinEventsDisabledByDefault() {
  Telemetry.clearEvents();
  Telemetry.canRecordExtended = true;

  const TEST_EVENT_NAME = "telemetry.test.offbydefault";

  // Register some dynamic builtin test events.
  Telemetry.registerBuiltinEvents(TEST_EVENT_NAME, {
    // Event with only required fields.
    test1: {
      methods: ["test1"],
      objects: ["object1"],
    },
  });

  // Record some events.
  // Explicitely _don't_ enable the category
  Telemetry.recordEvent(TEST_EVENT_NAME, "test1", "object1");

  // Now check that the snapshot contains the expected data.
  let snapshot = Telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    false
  );
  Assert.ok(
    !("parent" in snapshot),
    "Should not have parent events in the snapshot."
  );

  // Now enable the category and record again
  Telemetry.setEventRecordingEnabled(TEST_EVENT_NAME, true);
  Telemetry.recordEvent(TEST_EVENT_NAME, "test1", "object1");

  snapshot = Telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    false
  );
  Assert.ok("parent" in snapshot, "Should have parent events in the snapshot.");

  let expected = [[TEST_EVENT_NAME, "test1", "object1"]];
  let events = snapshot.parent;
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
});

add_task(async function test_dynamicBuiltinDontOverwriteStaticData() {
  Telemetry.clearEvents();
  Telemetry.canRecordExtended = true;

  const TEST_STATIC_EVENT_NAME = "telemetry.test";
  const TEST_EVENT_NAME = "telemetry.test.nooverwrite";

  // Register some dynamic builtin test events.
  Telemetry.registerBuiltinEvents(TEST_EVENT_NAME, {
    dynamic: {
      methods: ["dynamic"],
      objects: ["builtin", "anotherone"],
    },
  });

  // First enable the categories we're using
  Telemetry.setEventRecordingEnabled(TEST_STATIC_EVENT_NAME, true);
  Telemetry.setEventRecordingEnabled(TEST_EVENT_NAME, true);

  // Now record some dynamic-builtin and static events
  Telemetry.recordEvent(TEST_EVENT_NAME, "dynamic", "builtin");
  Telemetry.recordEvent(TEST_STATIC_EVENT_NAME, "test1", "object1");
  Telemetry.recordEvent(TEST_EVENT_NAME, "dynamic", "anotherone");

  let snapshot = Telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    false
  );
  Assert.ok("parent" in snapshot, "Should have parent events in the snapshot.");

  // All events should now be recorded in the right order
  let expected = [
    [TEST_EVENT_NAME, "dynamic", "builtin"],
    [TEST_STATIC_EVENT_NAME, "test1", "object1"],
    [TEST_EVENT_NAME, "dynamic", "anotherone"],
  ];
  let events = snapshot.parent;
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
});

add_task(async function test_dynamicBuiltinEventsOverridingStatic() {
  Telemetry.clearEvents();
  Telemetry.canRecordExtended = true;

  const TEST_EVENT_NAME = "telemetry.test";

  // Register dynamic builtin test events, overwriting existing one.
  Telemetry.registerBuiltinEvents(TEST_EVENT_NAME, {
    // Event with only required fields.
    test1: {
      methods: ["test1"],
      objects: ["object1", "object2"],
    },
    // Event with extra_keys.
    test2: {
      methods: ["test2"],
      objects: ["object1", "object2", "object3"],
      extra_keys: ["key1", "key2", "newdynamickey"],
    },
  });

  // Record some events that should be available in the static event already .
  Telemetry.setEventRecordingEnabled(TEST_EVENT_NAME, true);
  Telemetry.recordEvent(TEST_EVENT_NAME, "test1", "object1");
  Telemetry.recordEvent(TEST_EVENT_NAME, "test2", "object1", null, {
    key1: "foo",
    key2: "bar",
  });
  // Record events with newly added objects and keys.
  Telemetry.recordEvent(TEST_EVENT_NAME, "test2", "object2", null, {
    newdynamickey: "foo",
  });
  Telemetry.recordEvent(TEST_EVENT_NAME, "test2", "object3", null, {
    key1: "foo",
  });
  // Now check that the snapshot contains the expected data.
  let snapshot = Telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    false
  );
  Assert.ok("parent" in snapshot, "Should have parent events in the snapshot.");

  let expected = [
    [TEST_EVENT_NAME, "test1", "object1"],
    [TEST_EVENT_NAME, "test2", "object1", null, { key1: "foo", key2: "bar" }],
    [TEST_EVENT_NAME, "test2", "object2", null, { newdynamickey: "foo" }],
    [TEST_EVENT_NAME, "test2", "object3", null, { key1: "foo" }],
  ];
  let events = snapshot.parent;
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
});

add_task(async function test_realDynamicDontOverwrite() {
  // Real dynamic events follow similar code paths internally.
  // Let's ensure they trigger the right code path and don't overwrite.

  Telemetry.clearEvents();
  Telemetry.canRecordExtended = true;

  const TEST_EVENT_NAME = "telemetry.test";

  // Register dynamic test events, this should not overwrite existing ones.
  Telemetry.registerEvents(TEST_EVENT_NAME, {
    // Event with only required fields.
    test1: {
      methods: ["test1"],
      objects: ["object1", "object2"],
    },
    // Event with extra_keys.
    test2: {
      methods: ["test2"],
      objects: ["object1", "object2", "object3"],
      extra_keys: ["key1", "key2", "realdynamic"],
    },
  });

  // Record some events that should be available in the static event already .
  Telemetry.setEventRecordingEnabled(TEST_EVENT_NAME, true);
  Telemetry.recordEvent(TEST_EVENT_NAME, "test1", "object1");
  Telemetry.recordEvent(TEST_EVENT_NAME, "test2", "object1", null, {
    key1: "foo",
    key2: "bar",
  });
  // Record events with newly added objects and keys.
  Telemetry.recordEvent(TEST_EVENT_NAME, "test2", "object2", null, {
    realdynamic: "foo",
  });
  Telemetry.recordEvent(TEST_EVENT_NAME, "test2", "object3", null, {
    key1: "foo",
  });
  // Now check that the snapshot contains the expected data.
  let snapshot = Telemetry.snapshotEvents(
    Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
    false
  );
  Assert.ok("parent" in snapshot, "Should have parent events in the snapshot.");

  let expected = [
    [TEST_EVENT_NAME, "test1", "object1"],
    [TEST_EVENT_NAME, "test2", "object1", null, { key1: "foo", key2: "bar" }],
    [TEST_EVENT_NAME, "test2", "object3", null, { key1: "foo" }],
  ];
  let events = snapshot.parent;
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
});
