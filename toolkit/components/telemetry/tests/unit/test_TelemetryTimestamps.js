/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var Cu = Components.utils;
var Cc = Components.classes;
var Ci = Components.interfaces;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/TelemetryController.jsm", this);
Cu.import("resource://gre/modules/TelemetrySession.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

// The @mozilla/xre/app-info;1 XPCOM object provided by the xpcshell test harness doesn't
// implement the nsIXULAppInfo interface, which is needed by Services.jsm and
// TelemetrySession.jsm. updateAppInfo() creates and registers a minimal mock app-info.
Cu.import("resource://testing-common/AppInfo.jsm");
updateAppInfo();

var gGlobalScope = this;

function getSimpleMeasurementsFromTelemetryController() {
  return TelemetrySession.getPayload().simpleMeasurements;
}

add_task(async function test_setup() {
  // Telemetry needs the AddonManager.
  loadAddonManager();
  finishAddonManagerStartup();
  // Make profile available for |TelemetryController.testShutdown()|.
  do_get_profile();

  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();

  await new Promise(resolve =>
    Services.telemetry.asyncFetchTelemetryData(resolve));
});

add_task(async function actualTest() {
  await TelemetryController.testSetup();

  // Test the module logic
  let tmp = {};
  Cu.import("resource://gre/modules/TelemetryTimestamps.jsm", tmp);
  let TelemetryTimestamps = tmp.TelemetryTimestamps;
  let now = Date.now();
  TelemetryTimestamps.add("foo");
  Assert.ok(TelemetryTimestamps.get().foo != null); // foo was added
  Assert.ok(TelemetryTimestamps.get().foo >= now); // foo has a reasonable value

  // Add timestamp with value
  // Use a value far in the future since TelemetryController substracts the time of
  // process initialization.
  const YEAR_4000_IN_MS = 64060588800000;
  TelemetryTimestamps.add("bar", YEAR_4000_IN_MS);
  Assert.equal(TelemetryTimestamps.get().bar, YEAR_4000_IN_MS); // bar has the right value

  // Can't add the same timestamp twice
  TelemetryTimestamps.add("bar", 2);
  Assert.equal(TelemetryTimestamps.get().bar, YEAR_4000_IN_MS); // bar wasn't overwritten

  let threw = false;
  try {
    TelemetryTimestamps.add("baz", "this isn't a number");
  } catch (ex) {
    threw = true;
  }
  Assert.ok(threw); // adding non-number threw
  Assert.equal(null, TelemetryTimestamps.get().baz); // no baz was added

  // Test that the data gets added to the telemetry ping properly
  let simpleMeasurements = getSimpleMeasurementsFromTelemetryController();
  Assert.ok(simpleMeasurements != null); // got simple measurements from ping data
  Assert.ok(simpleMeasurements.foo > 1); // foo was included
  Assert.ok(simpleMeasurements.bar > 1); // bar was included
  Assert.equal(undefined, simpleMeasurements.baz); // baz wasn't included since it wasn't added

  await TelemetryController.testShutdown();
});
