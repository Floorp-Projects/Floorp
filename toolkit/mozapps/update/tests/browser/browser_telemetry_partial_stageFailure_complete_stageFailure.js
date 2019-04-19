/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Telemetry test for Application Update phases.
// Telemetry update.session
// Partial and complete patches
// Partial patch download
// Partial patch stage failure
// Complete patch download
// Complete patch stage failure
add_task(async function telemetry_partial_stageFailure_complete_stageFailure() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_APP_UPDATE_STAGING_ENABLED, true],
    ],
  });

  let updateParams = "";
  await runTelemetryUpdateTest(updateParams, "update-staged", true);

  await waitForEvent("update-staged");

  let expected = getTelemetryUpdatePhaseValues({
    forSession: true,
    noApplyPartial: true,
    noApplyComplete: true,
  });
  checkTelemetryUpdatePhases(expected);

  testPostUpdateProcessing();
  // Verify that update phase startup telemetry is empty.
  checkTelemetryUpdatePhaseEmpty(true);
});
