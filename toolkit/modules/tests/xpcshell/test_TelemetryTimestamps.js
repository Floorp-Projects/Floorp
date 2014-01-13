/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;
Cu.import("resource://gre/modules/Services.jsm");

// The @mozilla/xre/app-info;1 XPCOM object provided by the xpcshell test harness doesn't
// implement the nsIAppInfo interface, which is needed by Services.jsm and TelemetryPing.jsm.
// updateAppInfo() creates and registers a minimal mock app-info.
Cu.import("resource://testing-common/AppInfo.jsm");
updateAppInfo();

function getSimpleMeasurementsFromTelemetryPing() {
  return Cu.import("resource://gre/modules/TelemetryPing.jsm", {}).
    TelemetryPing.getPayload().simpleMeasurements;
}

function run_test() {
  do_test_pending();
  const Telemetry = Services.telemetry;
  Telemetry.asyncFetchTelemetryData(function () {
    try {
      actualTest();
    }
    catch(e) {
      do_throw("Failed: " + e);
    }
    do_test_finished();
  });
}

function actualTest() {
  // Test the module logic
  let tmp = {};
  Cu.import("resource://gre/modules/TelemetryTimestamps.jsm", tmp);
  let TelemetryTimestamps = tmp.TelemetryTimestamps;
  let now = Date.now();
  TelemetryTimestamps.add("foo");
  do_check_true(TelemetryTimestamps.get().foo != null); // foo was added
  do_check_true(TelemetryTimestamps.get().foo >= now); // foo has a reasonable value

  // Add timestamp with value
  // Use a value far in the future since TelemetryPing substracts the time of
  // process initialization.
  const YEAR_4000_IN_MS = 64060588800000;
  TelemetryTimestamps.add("bar", YEAR_4000_IN_MS);
  do_check_eq(TelemetryTimestamps.get().bar, YEAR_4000_IN_MS); // bar has the right value

  // Can't add the same timestamp twice
  TelemetryTimestamps.add("bar", 2);
  do_check_eq(TelemetryTimestamps.get().bar, YEAR_4000_IN_MS); // bar wasn't overwritten

  let threw = false;
  try {
    TelemetryTimestamps.add("baz", "this isn't a number");
  } catch (ex) {
    threw = true;
  }
  do_check_true(threw); // adding non-number threw
  do_check_null(TelemetryTimestamps.get().baz); // no baz was added

  // Test that the data gets added to the telemetry ping properly
  let simpleMeasurements = getSimpleMeasurementsFromTelemetryPing();
  do_check_true(simpleMeasurements != null); // got simple measurements from ping data
  do_check_true(simpleMeasurements.foo > 1); // foo was included
  do_check_true(simpleMeasurements.bar > 1); // bar was included
  do_check_null(simpleMeasurements.baz); // baz wasn't included since it wasn't added
}
