/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

"use strict";

ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm", this);
ChromeUtils.import("resource://testing-common/TelemetryArchiveTesting.jsm", this);

add_task(async function test_updatePing() {
  const TEST_VERSION = "37.85";
  const TEST_BUILDID = "20150711123724";

  // Set the preferences needed for the test: they will be cleared up
  // after it runs.
  await SpecialPowers.pushPrefEnv({"set": [
    [TelemetryUtils.Preferences.UpdatePing, true],
    ["app.update.postupdate", true],
    ["browser.startup.homepage_override.mstone", TEST_VERSION],
    ["browser.startup.homepage_override.buildID", TEST_BUILDID],
    ["toolkit.telemetry.log.level", "Trace"]
  ]});

  // Start monitoring the ping archive.
  let archiveChecker = new TelemetryArchiveTesting.Checker();
  await archiveChecker.promiseInit();

  // Manually call the BrowserContentHandler: this automatically gets called when
  // the browser is started and an update was applied successfully in order to
  // display the "update" info page.
  Cc["@mozilla.org/browser/clh;1"].getService(Ci.nsIBrowserHandler).defaultArgs;

  // We cannot control when the ping will be generated/archived after we trigger
  // an update, so let's make sure to have one before moving on with validation.
  let updatePing;
  await BrowserTestUtils.waitForCondition(async function() {
    // Check that the ping made it into the Telemetry archive.
    // The test data is defined in ../data/sharedUpdateXML.js
    updatePing = await archiveChecker.promiseFindPing("update", [
        [["payload", "reason"], "success"],
        [["payload", "previousBuildId"], TEST_BUILDID],
        [["payload", "previousVersion"], TEST_VERSION]
      ]);
    return !!updatePing;
  }, "Make sure the ping is generated before trying to validate it.", 500, 100);

  ok(updatePing, "The 'update' ping must be correctly sent.");

  // We have no easy way to simulate a previously applied update from toolkit/telemetry.
  // Instead of moving this test to mozapps/update as well, just test that the
  // "previousChannel" field is present and either a string or null.
  ok("previousChannel" in updatePing.payload,
     "The payload must contain the 'previousChannel' field");
  const channelField = updatePing.payload.previousChannel;
  if (channelField != null) {
    ok(typeof(channelField) == "string", "'previousChannel' must be a string, if available.");
  }

  // Also make sure that the ping contains both a client id and an
  // environment section.
  ok("clientId" in updatePing, "The update ping must report a client id.");
  ok("environment" in updatePing, "The update ping must report the environment.");
});
