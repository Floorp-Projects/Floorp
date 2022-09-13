/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported IS_OOP, valueSum, clearHistograms, getSnapshots, promiseTelemetryRecorded */

ChromeUtils.defineModuleGetter(
  this,
  "ContentTaskUtils",
  "resource://testing-common/ContentTaskUtils.jsm"
);

// Allows to run xpcshell telemetry test also on products (e.g. Thunderbird) where
// that telemetry wouldn't be actually collected in practice (but to be sure
// that it will work on those products as well by just adding the product in
// the telemetry metric definitions if it turns out we want to).
Services.prefs.setBoolPref(
  "toolkit.telemetry.testing.overrideProductsCheck",
  true
);

const IS_OOP = Services.prefs.getBoolPref("extensions.webextensions.remote");

const WEBEXT_EVENTPAGE_RUNNING_TIME_MS = "WEBEXT_EVENTPAGE_RUNNING_TIME_MS";
const WEBEXT_EVENTPAGE_RUNNING_TIME_MS_BY_ADDONID =
  "WEBEXT_EVENTPAGE_RUNNING_TIME_MS_BY_ADDONID";
const WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT = "WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT";
const WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT_BY_ADDONID =
  "WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT_BY_ADDONID";

// Keep this in sync with the order in Histograms.json for "WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT":
// the position of the category string determines the index of the values collected in the categorial
// histogram and so the existing labels should be kept in the exact same order and any new category
// to be added in the future should be appended to the existing ones.
const HISTOGRAM_EVENTPAGE_IDLE_RESULT_CATEGORIES = [
  "suspend",
  "reset_other",
  "reset_event",
  "reset_listeners",
  "reset_nativeapp",
  "reset_streamfilter",
];

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

function assertHistogramCategoryNotEmpty(
  histogramId,
  { category, categories, keyed, key },
  msg
) {
  let message = msg;

  if (!msg) {
    message = `Data recorded for histogram: ${histogramId}, category "${category}"`;
    if (keyed) {
      message += `, key "${key}"`;
    }
  }

  assertHistogramSnapshot(
    histogramId,
    {
      keyed,
      processSnapshot: snapshot => {
        const categoryIndex = categories.indexOf(category);
        if (keyed) {
          return {
            [key]: snapshot[key]
              ? snapshot[key].values[categoryIndex] > 0
              : null,
          };
        }
        return snapshot.values[categoryIndex] > 0;
      },
      expectedValue: keyed ? { [key]: true } : true,
    },
    message
  );
}
