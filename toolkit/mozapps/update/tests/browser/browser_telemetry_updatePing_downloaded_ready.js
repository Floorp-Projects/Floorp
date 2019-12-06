/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import(
  "resource://testing-common/TelemetryArchiveTesting.jsm",
  this
);

/**
 * Test that UpdatePing telemetry with a payload reason of ready is sent for a
 * staged update.
 *
 * Please note that this is really a Telemetry test, not an
 * "update UI" test like the rest of the tests in this directory.
 * This test does not live in toolkit/components/telemetry/tests to prevent
 * duplicating the code for all the test dependencies. Unfortunately, due
 * to a limitation in the build system, we were not able to simply reference
 * the dependencies as "support-files" in the test manifest.
 */
add_task(async function telemetry_updatePing_ready() {
  let archiveChecker = new TelemetryArchiveTesting.Checker();
  await archiveChecker.promiseInit();

  let updateParams = "";
  await runTelemetryUpdateTest(updateParams, "update-downloaded");

  // We cannot control when the ping will be generated/archived after we trigger
  // an update, so let's make sure to have one before moving on with validation.
  let updatePing;
  await TestUtils.waitForCondition(
    async function() {
      // Check that the ping made it into the Telemetry archive.
      // The test data is defined in ../data/sharedUpdateXML.js
      updatePing = await archiveChecker.promiseFindPing("update", [
        [["payload", "reason"], "ready"],
        [["payload", "targetBuildId"], "20080811053724"],
      ]);
      return !!updatePing;
    },
    "Make sure the ping is generated before trying to validate it.",
    500,
    100
  );

  ok(updatePing, "The 'update' ping must be correctly sent.");

  // We don't know the exact value for the other fields, so just check
  // that they're available.
  for (let f of ["targetVersion", "targetChannel", "targetDisplayVersion"]) {
    ok(
      f in updatePing.payload,
      `${f} must be available in the update ping payload.`
    );
    ok(
      typeof updatePing.payload[f] == "string",
      `${f} must have the correct format.`
    );
  }

  // Also make sure that the ping contains both a client id and an
  // environment section.
  ok("clientId" in updatePing, "The update ping must report a client id.");
  ok(
    "environment" in updatePing,
    "The update ping must report the environment."
  );
});
