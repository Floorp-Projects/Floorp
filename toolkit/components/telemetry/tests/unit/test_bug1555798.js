/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

ChromeUtils.defineModuleGetter(
  this,
  "TelemetryTestUtils",
  "resource://testing-common/TelemetryTestUtils.jsm"
);

add_task(async function test_bug1555798() {
  /*
  The idea behind this bug is that the registration of dynamic scalars causes
  the position of the scalarinfo for telemetry.dynamic_events_count to move
  which causes things to asplode.

  So to test this we'll be registering two dynamic events, recording to one of
  the events (to ensure the Scalar for event1 is allocated from the unmoved
  DynamicScalarInfo&), registering several dynamic scalars to cause the
  nsTArray of DynamicScalarInfo to realloc, and then recording to the second
  event to make the Event Summary Scalar for event2 try to allocate from where
  the DynamicScalarInfo used to be.
  */
  Telemetry.clearEvents();

  const DYNAMIC_CATEGORY = "telemetry.test.dynamic.event";
  Telemetry.registerEvents(DYNAMIC_CATEGORY, {
    an_event: {
      methods: ["a_method"],
      objects: ["an_object", "another_object"],
      record_on_release: true,
      expired: false,
    },
  });
  Telemetry.recordEvent(DYNAMIC_CATEGORY, "a_method", "an_object");

  for (let i = 0; i < 100; ++i) {
    Telemetry.registerScalars("telemetry.test.dynamic" + i, {
      scalar_name: {
        kind: Ci.nsITelemetry.SCALAR_TYPE_COUNT,
        record_on_release: true,
      },
    });
    Telemetry.scalarAdd("telemetry.test.dynamic" + i + ".scalar_name", 1);
  }

  Telemetry.recordEvent(DYNAMIC_CATEGORY, "a_method", "another_object");

  TelemetryTestUtils.assertNumberOfEvents(2, {}, { process: "dynamic" });
});
