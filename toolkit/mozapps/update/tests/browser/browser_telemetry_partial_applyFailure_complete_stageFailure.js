/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Telemetry test for Application Update phases.
// Telemetry update.session
// Partial and complete patches
// Partial patch download
// Partial patch apply failure
// Complete patch download
// Complete patch stage failure
add_task(async function telemetry_partial_applyFailure_complete_stageFailure() {
  let updateParams = "";
  await runTelemetryUpdateTest(updateParams, "update-downloaded");

  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_APP_UPDATE_STAGING_ENABLED, true],
    ],
  });
  // Now that staging is enabled setup the test updater.
  await setupTestUpdater();
  // Remove the update-settings.ini file so staging fails.
  removeUpdateSettingsIni();
  // Fail applying the partial.
  writeStatusFile(STATE_FAILED_CRC_ERROR);
  testPostUpdateProcessing();
  // Verify that update phase startup telemetry wasn't set.
  checkTelemetryUpdatePhaseEmpty(true);

  // The download and staging of the complete patch will happen automatically.
  await waitForEvent("update-staged");

  let expected = getTelemetryUpdatePhaseValues({
    forSession: true,
    noStagePartial: true,
    noApplyComplete: true,
  });
  checkTelemetryUpdatePhases(expected);

  testPostUpdateProcessing();
  // Verify that update phase startup telemetry is empty.
  checkTelemetryUpdatePhaseEmpty(true);
});
