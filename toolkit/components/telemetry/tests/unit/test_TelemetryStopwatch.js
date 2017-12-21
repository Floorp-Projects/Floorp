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

  Assert.ok(!TelemetryStopwatch.start(3));
  Assert.ok(!TelemetryStopwatch.start({}));
  Assert.ok(!TelemetryStopwatch.start("", 3));
  Assert.ok(!TelemetryStopwatch.start("", ""));
  Assert.ok(!TelemetryStopwatch.start({}, {}));

  Assert.ok(TelemetryStopwatch.start("mark1"));
  Assert.ok(TelemetryStopwatch.start("mark2"));

  Assert.ok(TelemetryStopwatch.start("mark1", refObj));
  Assert.ok(TelemetryStopwatch.start("mark2", refObj));

  // Same timer can't be re-started before being stopped
  Assert.ok(!TelemetryStopwatch.start("mark1"));
  Assert.ok(!TelemetryStopwatch.start("mark1", refObj));

  // Can't stop a timer that was accidentaly started twice
  Assert.ok(!TelemetryStopwatch.finish("mark1"));
  Assert.ok(!TelemetryStopwatch.finish("mark1", refObj));

  Assert.ok(TelemetryStopwatch.start("NON-EXISTENT_HISTOGRAM"));
  Assert.ok(!TelemetryStopwatch.finish("NON-EXISTENT_HISTOGRAM"));

  Assert.ok(TelemetryStopwatch.start("NON-EXISTENT_HISTOGRAM", refObj));
  Assert.ok(!TelemetryStopwatch.finish("NON-EXISTENT_HISTOGRAM", refObj));

  Assert.ok(!TelemetryStopwatch.running(HIST_NAME));
  Assert.ok(!TelemetryStopwatch.running(HIST_NAME2));
  Assert.ok(!TelemetryStopwatch.running(HIST_NAME, refObj));
  Assert.ok(!TelemetryStopwatch.running(HIST_NAME2, refObj));
  Assert.ok(!TelemetryStopwatch.running(HIST_NAME, refObj2));
  Assert.ok(!TelemetryStopwatch.running(HIST_NAME2, refObj2));

  Assert.ok(TelemetryStopwatch.start(HIST_NAME));
  Assert.ok(TelemetryStopwatch.start(HIST_NAME2));
  Assert.ok(TelemetryStopwatch.start(HIST_NAME, refObj));
  Assert.ok(TelemetryStopwatch.start(HIST_NAME2, refObj));
  Assert.ok(TelemetryStopwatch.start(HIST_NAME, refObj2));
  Assert.ok(TelemetryStopwatch.start(HIST_NAME2, refObj2));

  Assert.ok(TelemetryStopwatch.running(HIST_NAME));
  Assert.ok(TelemetryStopwatch.running(HIST_NAME));
  Assert.ok(TelemetryStopwatch.running(HIST_NAME2));
  Assert.ok(TelemetryStopwatch.running(HIST_NAME, refObj));
  Assert.ok(TelemetryStopwatch.running(HIST_NAME2, refObj));
  Assert.ok(TelemetryStopwatch.running(HIST_NAME, refObj2));
  Assert.ok(TelemetryStopwatch.running(HIST_NAME2, refObj2));

  Assert.ok(TelemetryStopwatch.finish(HIST_NAME));
  Assert.ok(TelemetryStopwatch.finish(HIST_NAME2));
  Assert.ok(TelemetryStopwatch.finish(HIST_NAME, refObj));
  Assert.ok(TelemetryStopwatch.finish(HIST_NAME2, refObj));
  Assert.ok(TelemetryStopwatch.finish(HIST_NAME, refObj2));
  Assert.ok(TelemetryStopwatch.finish(HIST_NAME2, refObj2));

  Assert.ok(!TelemetryStopwatch.running(HIST_NAME));
  Assert.ok(!TelemetryStopwatch.running(HIST_NAME));
  Assert.ok(!TelemetryStopwatch.running(HIST_NAME2));
  Assert.ok(!TelemetryStopwatch.running(HIST_NAME, refObj));
  Assert.ok(!TelemetryStopwatch.running(HIST_NAME2, refObj));
  Assert.ok(!TelemetryStopwatch.running(HIST_NAME, refObj2));
  Assert.ok(!TelemetryStopwatch.running(HIST_NAME2, refObj2));

  // Verify that TS.finish deleted the timers
  Assert.ok(!TelemetryStopwatch.finish(HIST_NAME));
  Assert.ok(!TelemetryStopwatch.finish(HIST_NAME, refObj));

  // Verify that they can be used again
  Assert.ok(TelemetryStopwatch.start(HIST_NAME));
  Assert.ok(TelemetryStopwatch.start(HIST_NAME, refObj));
  Assert.ok(TelemetryStopwatch.finish(HIST_NAME));
  Assert.ok(TelemetryStopwatch.finish(HIST_NAME, refObj));

  Assert.ok(!TelemetryStopwatch.finish("unknown-mark")); // Unknown marker
  Assert.ok(!TelemetryStopwatch.finish("unknown-mark", {})); // Unknown object
  Assert.ok(!TelemetryStopwatch.finish(HIST_NAME, {})); // Known mark on unknown object

  // Test cancel
  Assert.ok(!TelemetryStopwatch.running(HIST_NAME));
  Assert.ok(!TelemetryStopwatch.running(HIST_NAME, refObj));
  Assert.ok(TelemetryStopwatch.start(HIST_NAME));
  Assert.ok(TelemetryStopwatch.start(HIST_NAME, refObj));
  Assert.ok(TelemetryStopwatch.running(HIST_NAME));
  Assert.ok(TelemetryStopwatch.running(HIST_NAME, refObj));
  Assert.ok(TelemetryStopwatch.cancel(HIST_NAME));
  Assert.ok(TelemetryStopwatch.cancel(HIST_NAME, refObj));

  // Verify that can not cancel twice
  Assert.ok(!TelemetryStopwatch.cancel(HIST_NAME));
  Assert.ok(!TelemetryStopwatch.cancel(HIST_NAME, refObj));

  // Verify that cancel removes the timers
  Assert.ok(!TelemetryStopwatch.running(HIST_NAME));
  Assert.ok(!TelemetryStopwatch.running(HIST_NAME, refObj));
  Assert.ok(!TelemetryStopwatch.finish(HIST_NAME));
  Assert.ok(!TelemetryStopwatch.finish(HIST_NAME, refObj));

  // Verify that keyed stopwatch reject invalid keys.
  for (let key of [3, {}, ""]) {
    Assert.ok(!TelemetryStopwatch.startKeyed(KEYED_HIST.id, key));
  }

  // Verify that keyed histograms can be started.
  Assert.ok(!TelemetryStopwatch.runningKeyed("HISTOGRAM", "KEY1"));
  Assert.ok(!TelemetryStopwatch.runningKeyed("HISTOGRAM", "KEY2"));
  Assert.ok(!TelemetryStopwatch.runningKeyed("HISTOGRAM", "KEY1", refObj));
  Assert.ok(!TelemetryStopwatch.runningKeyed("HISTOGRAM", "KEY2", refObj));

  Assert.ok(TelemetryStopwatch.startKeyed("HISTOGRAM", "KEY1"));
  Assert.ok(TelemetryStopwatch.startKeyed("HISTOGRAM", "KEY2"));
  Assert.ok(TelemetryStopwatch.startKeyed("HISTOGRAM", "KEY1", refObj));
  Assert.ok(TelemetryStopwatch.startKeyed("HISTOGRAM", "KEY2", refObj));

  Assert.ok(TelemetryStopwatch.runningKeyed("HISTOGRAM", "KEY1"));
  Assert.ok(TelemetryStopwatch.runningKeyed("HISTOGRAM", "KEY2"));
  Assert.ok(TelemetryStopwatch.runningKeyed("HISTOGRAM", "KEY1", refObj));
  Assert.ok(TelemetryStopwatch.runningKeyed("HISTOGRAM", "KEY2", refObj));

  // Restarting keyed histograms should fail.
  Assert.ok(!TelemetryStopwatch.startKeyed("HISTOGRAM", "KEY1"));
  Assert.ok(!TelemetryStopwatch.startKeyed("HISTOGRAM", "KEY1", refObj));

  // Finishing a stopwatch of a non existing histogram should return false.
  Assert.ok(!TelemetryStopwatch.finishKeyed("HISTOGRAM", "KEY2"));
  Assert.ok(!TelemetryStopwatch.finishKeyed("HISTOGRAM", "KEY2", refObj));

  // Starting & finishing a keyed stopwatch for an existing histogram should work.
  Assert.ok(TelemetryStopwatch.startKeyed(KEYED_HIST.id, KEYED_HIST.key));
  Assert.ok(TelemetryStopwatch.finishKeyed(KEYED_HIST.id, KEYED_HIST.key));
  // Verify that TS.finish deleted the timers
  Assert.ok(!TelemetryStopwatch.runningKeyed(KEYED_HIST.id, KEYED_HIST.key));

  // Verify that they can be used again
  Assert.ok(TelemetryStopwatch.startKeyed(KEYED_HIST.id, KEYED_HIST.key));
  Assert.ok(TelemetryStopwatch.finishKeyed(KEYED_HIST.id, KEYED_HIST.key));

  Assert.ok(!TelemetryStopwatch.finishKeyed("unknown-mark", "unknown-key"));
  Assert.ok(!TelemetryStopwatch.finishKeyed(KEYED_HIST.id, "unknown-key"));

  // Verify that keyed histograms can only be canceled through "keyed" API.
  Assert.ok(TelemetryStopwatch.startKeyed(KEYED_HIST.id, KEYED_HIST.key));
  Assert.ok(!TelemetryStopwatch.cancel(KEYED_HIST.id, KEYED_HIST.key));
  Assert.ok(TelemetryStopwatch.cancelKeyed(KEYED_HIST.id, KEYED_HIST.key));
  Assert.ok(!TelemetryStopwatch.cancelKeyed(KEYED_HIST.id, KEYED_HIST.key));

  finishTest();
}

function finishTest() {
  let histogram = Telemetry.getHistogramById(HIST_NAME);
  let snapshot = histogram.snapshot();
  let newCount = snapshot.counts.reduce((a, b) => a += b);

  Assert.equal(newCount - originalCount1, 5, "The correct number of histograms were added for histogram 1.");

  histogram = Telemetry.getHistogramById(HIST_NAME2);
  snapshot = histogram.snapshot();
  newCount = snapshot.counts.reduce((a, b) => a += b);

  Assert.equal(newCount - originalCount2, 3, "The correct number of histograms were added for histogram 2.");

  histogram = Telemetry.getKeyedHistogramById(KEYED_HIST.id);
  snapshot = histogram.snapshot(KEYED_HIST.key);
  newCount = snapshot.counts.reduce((a, b) => a += b);

  Assert.equal(newCount - originalCount3, 2, "The correct number of histograms were added for histogram 3.");
}
