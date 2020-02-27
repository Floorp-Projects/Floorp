/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Telemetry test for Application Update phases.
// Telemetry update.startup
// Complete patch only
// Complete patch download
// Complete patch applied
add_task(async function telemetry_complete_applied() {
  let updateParams = "&completePatchOnly=1";
  await runTelemetryUpdateTest(updateParams, "update-downloaded");

  writeStatusFile(STATE_SUCCEEDED);
  testPostUpdateProcessing();

  let expected = getTelemetryUpdatePhaseValues({
    noPartialPatch: true,
  });
  checkTelemetryUpdatePhases(expected);

  // Verify that update phase session telemetry is empty.
  checkTelemetryUpdatePhaseEmpty(false);
});
