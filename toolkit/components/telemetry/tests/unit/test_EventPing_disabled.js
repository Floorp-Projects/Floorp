/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

ChromeUtils.import("resource://gre/modules/TelemetryController.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryStorage.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Preferences.jsm", this);
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);
ChromeUtils.import(
  "resource://testing-common/TelemetryArchiveTesting.jsm",
  this
);

ChromeUtils.defineModuleGetter(
  this,
  "TelemetryEventPing",
  "resource://gre/modules/EventPing.jsm"
);

const PRERELEASE_CHANNELS = Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS;

add_task(async function setup() {
  Services.prefs.setBoolPref(
    TelemetryUtils.Preferences.EventPingEnabled,
    false
  );
  // Trigger a proper telemetry init.
  do_get_profile(true);
  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();

  await TelemetryController.testSetup();
  TelemetryEventPing.testReset();
  Telemetry.setEventRecordingEnabled("telemetry.test", true);
});

add_task(async function test_EventsNotRecorded() {
  Telemetry.recordEvent("telemetry.test", "test1", "object1");
  let snapshot = Telemetry.snapshotEvents(PRERELEASE_CHANNELS, true);
  Assert.equal(
    Object.keys(snapshot).length,
    0,
    "Should not have recorded any events."
  );
});
