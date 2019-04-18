/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Telemetry test for Application Update phases.
// Telemetry update.startup
// Partial and complete patches
// Partial patch download
// Partial patch apply failure
// Complete patch download
// Complete patch apply failure
add_task(async function telemetry_partial_applyFailure_complete_applyFailure() {
  let updateParams = "";
  await runTelemetryUpdateTest(updateParams, "update-downloaded");

  writeStatusFile(STATE_FAILED_CRC_ERROR);
  testPostUpdateProcessing();
  // Verify that update phase startup telemetry is empty.
  checkTelemetryUpdatePhaseEmpty(true);

  // The download of the complete patch will happen automatically.
  await waitForEvent("update-downloaded");
  writeStatusFile(STATE_FAILED_CRC_ERROR);
  testPostUpdateProcessing();

  let expected = getTelemetryUpdatePhaseValues({});
  checkTelemetryUpdatePhases(expected);

  // Verify that update phase session telemetry is empty.
  checkTelemetryUpdatePhaseEmpty(false);
});
