"use strict";

ChromeUtils.import("resource://gre/modules/TelemetryController.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetrySession.jsm", this);

add_task(async function test() {
  let now = Services.telemetry.msSinceProcessStart();
  let payload = TelemetrySession.getPayload("main");

  // Check the first_paint scalar.
  ok(
    "scalars" in payload.processes.parent,
    "Scalars are present in the payload."
  );
  ok(
    "timestamps.first_paint" in payload.processes.parent.scalars,
    "The first_paint timestamp is present in the payload."
  );
  Assert.greater(
    payload.processes.parent.scalars["timestamps.first_paint"],
    0,
    "first_paint scalar is greater than 0."
  );
  Assert.greater(now, 0, "Browser test runtime is greater than zero.");
  // Check that the first_paint scalar is less than the current time.
  Assert.greater(
    now,
    payload.processes.parent.scalars["timestamps.first_paint"],
    "first_paint is less than total browser test runtime."
  );
});
