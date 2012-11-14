/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const Telemetry = Cc["@mozilla.org/base/telemetry;1"]
                  .getService(Ci.nsITelemetry);

let tmpScope = {};
Cu.import("resource://gre/modules/TelemetryStopwatch.jsm", tmpScope);
let TelemetryStopwatch = tmpScope.TelemetryStopwatch;

// We can't create a histogram here since the ones created with
// newHistogram are not seen by getHistogramById that the module uses.
const HIST_NAME = "TELEMETRY_PING";
const HIST_NAME2 = "RANGE_CHECKSUM_ERRORS";

let refObj = {}, refObj2 = {};

let originalCount1, originalCount2;

function run_test() {
  let histogram = Telemetry.getHistogramById(HIST_NAME);
  let snapshot = histogram.snapshot();
  originalCount1 = snapshot.counts.reduce(function (a,b) a += b);

  histogram = Telemetry.getHistogramById(HIST_NAME2);
  snapshot = histogram.snapshot();
  originalCount2 = snapshot.counts.reduce(function (a,b) a += b);

  do_check_false(TelemetryStopwatch.start(3));
  do_check_false(TelemetryStopwatch.start({}));
  do_check_false(TelemetryStopwatch.start("", 3));
  do_check_false(TelemetryStopwatch.start("", ""));
  do_check_false(TelemetryStopwatch.start({}, {}));

  do_check_true(TelemetryStopwatch.start("mark1"));
  do_check_true(TelemetryStopwatch.start("mark2"));

  do_check_true(TelemetryStopwatch.start("mark1", refObj));
  do_check_true(TelemetryStopwatch.start("mark2", refObj));

  // Same timer can't be re-started before being stopped
  do_check_false(TelemetryStopwatch.start("mark1"));
  do_check_false(TelemetryStopwatch.start("mark1", refObj));

  // Can't stop a timer that was accidentaly started twice
  do_check_false(TelemetryStopwatch.finish("mark1"));
  do_check_false(TelemetryStopwatch.finish("mark1", refObj));

  do_check_true(TelemetryStopwatch.start("NON-EXISTENT_HISTOGRAM"));
  try {
    TelemetryStopwatch.finish("NON-EXISTENT_HISTOGRAM");
    do_throw("Non-existent histogram name should throw an error.");
  } catch (e) {}

  do_check_true(TelemetryStopwatch.start("NON-EXISTENT_HISTOGRAM", refObj));
  try {
    TelemetryStopwatch.finish("NON-EXISTENT_HISTOGRAM", refObj);
    do_throw("Non-existent histogram name should throw an error.");
  } catch (e) {}

  do_check_true(TelemetryStopwatch.start(HIST_NAME));
  do_check_true(TelemetryStopwatch.start(HIST_NAME2));
  do_check_true(TelemetryStopwatch.start(HIST_NAME, refObj));
  do_check_true(TelemetryStopwatch.start(HIST_NAME2, refObj));
  do_check_true(TelemetryStopwatch.start(HIST_NAME, refObj2));
  do_check_true(TelemetryStopwatch.start(HIST_NAME2, refObj2));

  do_check_true(TelemetryStopwatch.finish(HIST_NAME));
  do_check_true(TelemetryStopwatch.finish(HIST_NAME2));
  do_check_true(TelemetryStopwatch.finish(HIST_NAME, refObj));
  do_check_true(TelemetryStopwatch.finish(HIST_NAME2, refObj));
  do_check_true(TelemetryStopwatch.finish(HIST_NAME, refObj2));
  do_check_true(TelemetryStopwatch.finish(HIST_NAME2, refObj2));

  // Verify that TS.finish deleted the timers
  do_check_false(TelemetryStopwatch.finish(HIST_NAME));
  do_check_false(TelemetryStopwatch.finish(HIST_NAME, refObj));

  // Verify that they can be used again
  do_check_true(TelemetryStopwatch.start(HIST_NAME));
  do_check_true(TelemetryStopwatch.start(HIST_NAME, refObj));
  do_check_true(TelemetryStopwatch.finish(HIST_NAME));
  do_check_true(TelemetryStopwatch.finish(HIST_NAME, refObj));

  do_check_false(TelemetryStopwatch.finish("unknown-mark")); // Unknown marker
  do_check_false(TelemetryStopwatch.finish("unknown-mark", {})); // Unknown object
  do_check_false(TelemetryStopwatch.finish(HIST_NAME, {})); // Known mark on unknown object

  // Test cancel
  do_check_true(TelemetryStopwatch.start(HIST_NAME));
  do_check_true(TelemetryStopwatch.start(HIST_NAME, refObj));
  do_check_true(TelemetryStopwatch.cancel(HIST_NAME));
  do_check_true(TelemetryStopwatch.cancel(HIST_NAME, refObj));

  // Verify that can not cancel twice
  do_check_false(TelemetryStopwatch.cancel(HIST_NAME));
  do_check_false(TelemetryStopwatch.cancel(HIST_NAME, refObj));

  // Verify that cancel removes the timers
  do_check_false(TelemetryStopwatch.finish(HIST_NAME));
  do_check_false(TelemetryStopwatch.finish(HIST_NAME, refObj));

  finishTest();
}

function finishTest() {
  let histogram = Telemetry.getHistogramById(HIST_NAME);
  let snapshot = histogram.snapshot();
  let newCount = snapshot.counts.reduce(function (a,b) a += b);

  do_check_eq(newCount - originalCount1, 5, "The correct number of histograms were added for histogram 1.");

  histogram = Telemetry.getHistogramById(HIST_NAME2);
  snapshot = histogram.snapshot();
  newCount = snapshot.counts.reduce(function (a,b) a += b);

  do_check_eq(newCount - originalCount2, 3, "The correct number of histograms were added for histogram 2.");
}
