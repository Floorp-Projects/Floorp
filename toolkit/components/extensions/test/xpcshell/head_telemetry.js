/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported IS_OOP, valueSum, clearHistograms, getSnapshots, promiseTelemetryRecorded */

ChromeUtils.defineModuleGetter(
  this,
  "ContentTaskUtils",
  "resource://testing-common/ContentTaskUtils.jsm"
);

const IS_OOP = Services.prefs.getBoolPref("extensions.webextensions.remote");

function valueSum(arr) {
  return Object.values(arr).reduce((a, b) => a + b, 0);
}

function clearHistograms() {
  Services.telemetry.getSnapshotForHistograms("main", true /* clear */);
  Services.telemetry.getSnapshotForKeyedHistograms("main", true /* clear */);
}

function getSnapshots(process) {
  return Services.telemetry.getSnapshotForHistograms("main", false /* clear */)[
    process
  ];
}

function getKeyedSnapshots(process) {
  return Services.telemetry.getSnapshotForKeyedHistograms(
    "main",
    false /* clear */
  )[process];
}

// TODO Bug 1357509: There is no good way to make sure that the parent received
// the histogram entries from the extension and content processes.  Let's stick
// to the ugly, spinning the event loop until we have a good approach.
function promiseTelemetryRecorded(id, process, expectedCount) {
  let condition = () => {
    let snapshot = Services.telemetry.getSnapshotForHistograms(
      "main",
      false /* clear */
    )[process][id];
    return snapshot && valueSum(snapshot.values) >= expectedCount;
  };
  return ContentTaskUtils.waitForCondition(condition);
}

function promiseKeyedTelemetryRecorded(
  id,
  process,
  expectedKey,
  expectedCount
) {
  let condition = () => {
    let snapshot = Services.telemetry.getSnapshotForKeyedHistograms(
      "main",
      false /* clear */
    )[process][id];
    return (
      snapshot &&
      snapshot[expectedKey] &&
      valueSum(snapshot[expectedKey].values) >= expectedCount
    );
  };
  return ContentTaskUtils.waitForCondition(condition);
}

function assertHistogramSnapshot(
  histogramId,
  { keyed, processSnapshot, expectedValue },
  msg
) {
  let histogram;

  if (keyed) {
    histogram = Services.telemetry.getKeyedHistogramById(histogramId);
  } else {
    histogram = Services.telemetry.getHistogramById(histogramId);
  }

  let res = processSnapshot(histogram.snapshot());
  Assert.deepEqual(res, expectedValue, msg);
  return res;
}

function assertHistogramEmpty(histogramId) {
  assertHistogramSnapshot(
    histogramId,
    {
      processSnapshot: snapshot => snapshot.sum,
      expectedValue: 0,
    },
    `No data recorded for histogram: ${histogramId}.`
  );
}

function assertKeyedHistogramEmpty(histogramId) {
  assertHistogramSnapshot(
    histogramId,
    {
      keyed: true,
      processSnapshot: snapshot => Object.keys(snapshot).length,
      expectedValue: 0,
    },
    `No data recorded for histogram: ${histogramId}.`
  );
}
