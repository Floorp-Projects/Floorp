const { UptakeTelemetry } = Cu.import("resource://services-common/uptake-telemetry.js", {});


function run_test() {
  run_next_test();
}

add_task(async function test_unknown_status_is_not_reported() {
  const source = "update-source";
  const startHistogram = getUptakeTelemetrySnapshot(source);

  UptakeTelemetry.report(source, "unknown-status");

  const endHistogram = getUptakeTelemetrySnapshot(source);
  const expectedIncrements = {};
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});

add_task(async function test_each_status_can_be_caught_in_snapshot() {
  const source = "some-source";
  const startHistogram = getUptakeTelemetrySnapshot(source);

  const expectedIncrements = {};
  for (const label of Object.keys(UptakeTelemetry.STATUS)) {
    const status = UptakeTelemetry.STATUS[label];
    UptakeTelemetry.report(source, status);
    expectedIncrements[status] = 1;
  }

  const endHistogram = getUptakeTelemetrySnapshot(source);
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
