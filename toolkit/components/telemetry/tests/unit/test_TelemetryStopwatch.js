/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var tmpScope = {};
Cu.import("resource://gre/modules/TelemetryStopwatch.jsm", tmpScope);
var TelemetryStopwatch = tmpScope.TelemetryStopwatch;

const HIST_NAME = "TELEMETRY_SEND_SUCCESS";
const HIST_NAME2 = "RANGE_CHECKSUM_ERRORS";
const KEYED_HIST = { id: "TELEMETRY_INVALID_PING_TYPE_SUBMITTED", key: "TEST" };

var refObj = {}, refObj2 = {};

var originalCount1, originalCount2, originalCount3;

function run_test() {
  let histogram = Telemetry.getHistogramById(HIST_NAME);
  let snapshot = histogram.snapshot();
  originalCount1 = snapshot.counts.reduce((a, b) => a += b);

  histogram = Telemetry.getHistogramById(HIST_NAME2);
  snapshot = histogram.snapshot();
  originalCount2 = snapshot.counts.reduce((a, b) => a += b);

  histogram = Telemetry.getKeyedHistogramById(KEYED_HIST.id);
  snapshot = histogram.snapshot(KEYED_HIST.key);
  originalCount3 = snapshot.counts.reduce((a, b) => a += b);

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
  do_check_false(TelemetryStopwatch.finish("NON-EXISTENT_HISTOGRAM"));

  do_check_true(TelemetryStopwatch.start("NON-EXISTENT_HISTOGRAM", refObj));
  do_check_false(TelemetryStopwatch.finish("NON-EXISTENT_HISTOGRAM", refObj));

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

  // Verify that keyed stopwatch reject invalid keys.
  for (let key of [3, {}, ""]) {
    do_check_false(TelemetryStopwatch.startKeyed(KEYED_HIST.id, key));
  }

  // Verify that keyed histograms can be started.
  do_check_true(TelemetryStopwatch.startKeyed("HISTOGRAM", "KEY1"));
  do_check_true(TelemetryStopwatch.startKeyed("HISTOGRAM", "KEY2"));
  do_check_true(TelemetryStopwatch.startKeyed("HISTOGRAM", "KEY1", refObj));
  do_check_true(TelemetryStopwatch.startKeyed("HISTOGRAM", "KEY2", refObj));

  // Restarting keyed histograms should fail.
  do_check_false(TelemetryStopwatch.startKeyed("HISTOGRAM", "KEY1"));
  do_check_false(TelemetryStopwatch.startKeyed("HISTOGRAM", "KEY1", refObj));

  // Finishing a stopwatch of a non existing histogram should return false.
  do_check_false(TelemetryStopwatch.finishKeyed("HISTOGRAM", "KEY2"));
  do_check_false(TelemetryStopwatch.finishKeyed("HISTOGRAM", "KEY2", refObj));

  // Starting & finishing a keyed stopwatch for an existing histogram should work.
  do_check_true(TelemetryStopwatch.startKeyed(KEYED_HIST.id, KEYED_HIST.key));
  do_check_true(TelemetryStopwatch.finishKeyed(KEYED_HIST.id, KEYED_HIST.key));
  // Verify that TS.finish deleted the timers
  do_check_false(TelemetryStopwatch.finishKeyed(KEYED_HIST.id, KEYED_HIST.key));

  // Verify that they can be used again
  do_check_true(TelemetryStopwatch.startKeyed(KEYED_HIST.id, KEYED_HIST.key));
  do_check_true(TelemetryStopwatch.finishKeyed(KEYED_HIST.id, KEYED_HIST.key));

  do_check_false(TelemetryStopwatch.finishKeyed("unknown-mark", "unknown-key"));
  do_check_false(TelemetryStopwatch.finishKeyed(KEYED_HIST.id, "unknown-key"));

  // Verify that keyed histograms can only be canceled through "keyed" API.
  do_check_true(TelemetryStopwatch.startKeyed(KEYED_HIST.id, KEYED_HIST.key));
  do_check_false(TelemetryStopwatch.cancel(KEYED_HIST.id, KEYED_HIST.key));
  do_check_true(TelemetryStopwatch.cancelKeyed(KEYED_HIST.id, KEYED_HIST.key));
  do_check_false(TelemetryStopwatch.cancelKeyed(KEYED_HIST.id, KEYED_HIST.key));

  finishTest();
}

function finishTest() {
  let histogram = Telemetry.getHistogramById(HIST_NAME);
  let snapshot = histogram.snapshot();
  let newCount = snapshot.counts.reduce((a, b) => a += b);

  do_check_eq(newCount - originalCount1, 5, "The correct number of histograms were added for histogram 1.");

  histogram = Telemetry.getHistogramById(HIST_NAME2);
  snapshot = histogram.snapshot();
  newCount = snapshot.counts.reduce((a, b) => a += b);

  do_check_eq(newCount - originalCount2, 3, "The correct number of histograms were added for histogram 2.");

  histogram = Telemetry.getKeyedHistogramById(KEYED_HIST.id);
  snapshot = histogram.snapshot(KEYED_HIST.key);
  newCount = snapshot.counts.reduce((a, b) => a += b);

  do_check_eq(newCount - originalCount3, 2, "The correct number of histograms were added for histogram 3.");
}
