/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Telemetry test for Application Update phases.
// Telemetry update.session
// Complete patch only
// Complete patch download
// Complete patch stage failure
add_task(async function telemetry_complete_stageFailure() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_APP_UPDATE_STAGING_ENABLED, true],
    ],
  });

  let updateParams = "&completePatchOnly=1";
  await runTelemetryUpdateTest(updateParams, "update-staged", true);

  let expected = getTelemetryUpdatePhaseValues({
    forSession: true,
    noPartialPatch: true,
    noApplyComplete: true,
  });
  checkTelemetryUpdatePhases(expected);

  testPostUpdateProcessing();
  // Verify that update phase startup telemetry is empty.
  checkTelemetryUpdatePhaseEmpty(true);
});
