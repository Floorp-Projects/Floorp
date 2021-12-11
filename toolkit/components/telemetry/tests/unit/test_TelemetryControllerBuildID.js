/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/
/* Test inclusion of previous build ID in telemetry pings when build ID changes.
 * bug 841028
 *
 * Cases to cover:
 * 1) Run with no "previousBuildID" stored in prefs:
 *     -> no previousBuildID in telemetry system info, new value set in prefs.
 * 2) previousBuildID in prefs, equal to current build ID:
 *     -> no previousBuildID in telemetry, prefs not updated.
 * 3) previousBuildID in prefs, not equal to current build ID:
 *     -> previousBuildID in telemetry, new value set in prefs.
 */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { TelemetryController } = ChromeUtils.import(
  "resource://gre/modules/TelemetryController.jsm"
);
const { TelemetrySession } = ChromeUtils.import(
  "resource://gre/modules/TelemetrySession.jsm"
);

// Set up our dummy AppInfo object so we can control the appBuildID.
const { getAppInfo, updateAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
);
updateAppInfo();

// Check that when run with no previous build ID stored, we update the pref but do not
// put anything into the metadata.
add_task(async function test_firstRun() {
  await TelemetryController.testReset();
  let metadata = TelemetrySession.getMetadata();
  Assert.equal(false, "previousBuildID" in metadata);
  let appBuildID = getAppInfo().appBuildID;
  let buildIDPref = Services.prefs.getCharPref(
    TelemetryUtils.Preferences.PreviousBuildID
  );
  Assert.equal(appBuildID, buildIDPref);
});

// Check that a subsequent run with the same build ID does not put prev build ID in
// metadata. Assumes testFirstRun() has already been called to set the previousBuildID pref.
add_task(async function test_secondRun() {
  await TelemetryController.testReset();
  let metadata = TelemetrySession.getMetadata();
  Assert.equal(false, "previousBuildID" in metadata);
});

// Set up telemetry with a different app build ID and check that the old build ID
// is returned in the metadata and the pref is updated to the new build ID.
// Assumes testFirstRun() has been called to set the previousBuildID pref.
const NEW_BUILD_ID = "20130314";
add_task(async function test_newBuild() {
  let info = getAppInfo();
  let oldBuildID = info.appBuildID;
  info.appBuildID = NEW_BUILD_ID;
  await TelemetryController.testReset();
  let metadata = TelemetrySession.getMetadata();
  Assert.equal(metadata.previousBuildId, oldBuildID);
  let buildIDPref = Services.prefs.getCharPref(
    TelemetryUtils.Preferences.PreviousBuildID
  );
  Assert.equal(NEW_BUILD_ID, buildIDPref);
});

function run_test() {
  // Make sure we have a profile directory.
  do_get_profile();

  run_next_test();
}
