/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Telemetry test for Application Update phases.
// Telemetry update.session
// Complete patch only
// Complete patch download failure
add_task(async function telemetry_completeBadSize() {
  let updateParams = "&completePatchOnly=1&invalidCompleteSize=1";
  await runTelemetryUpdateTest(updateParams, "update-error");

  let expected = getTelemetryUpdatePhaseValues({
    forSession: true,
    noPartialPatch: true,
    completeBadSize: true,
  });
  checkTelemetryUpdatePhases(expected);

  testPostUpdateProcessing();
  // Verify that update phase startup telemetry is empty.
  checkTelemetryUpdatePhaseEmpty(true);
});
