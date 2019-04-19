/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Telemetry test for Application Update phases.
// Telemetry update.startup
// Partial and complete patches
// Partial patch download
// Partial patch apply failure
// Complete patch download
// Complete patch staged
// Complete patch applied
add_task(async function telemetry_partial_applyFailure_complete_staged_applied() {
  let updateParams = "";
  await runTelemetryUpdateTest(updateParams, "update-downloaded");

  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_APP_UPDATE_STAGING_ENABLED, true],
    ],
  });
  // Now that staging is enabled setup the test updater.
  await setupTestUpdater();
  // Fail applying the partial.
  writeStatusFile(STATE_FAILED_CRC_ERROR);
  testPostUpdateProcessing();
  // Verify that update phase startup telemetry is empty.
  checkTelemetryUpdatePhaseEmpty(true);

  // The download and staging of the complete will happen automatically.
  await waitForEvent("update-staged");
  // Succeed applying the complete patch.
  writeStatusFile(STATE_SUCCEEDED);
  testPostUpdateProcessing();

  let expected = getTelemetryUpdatePhaseValues({
    noStagePartial: true,
  });
  checkTelemetryUpdatePhases(expected);

  // Verify that update phase session telemetry is empty.
  checkTelemetryUpdatePhaseEmpty(false);
});
