/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported IS_OOP, arraySum, clearHistograms, getSnapshots, promiseTelemetryRecorded */

XPCOMUtils.defineLazyModuleGetter(this, "ContentTaskUtils",
                                  "resource://testing-common/ContentTaskUtils.jsm");

const IS_OOP = Services.prefs.getBoolPref("extensions.webextensions.remote");

function arraySum(arr) {
  return arr.reduce((a, b) => a + b, 0);
}

function clearHistograms() {
  Services.telemetry.snapshotSubsessionHistograms(true);
}

function getSnapshots(process) {
  return Services.telemetry.snapshotSubsessionHistograms()[process];
}

// There is no good way to make sure that the parent received the histogram
// entries from the extension and content processes.
// Let's stick to the ugly, spinning the event loop until we have a good
// approach (Bug 1357509).
function promiseTelemetryRecorded(id, process, expectedCount) {
  let condition = () => {
    let snapshot = Services.telemetry.snapshotSubsessionHistograms()[process][id];
    return snapshot && arraySum(snapshot.counts) >= expectedCount;
  };
  return ContentTaskUtils.waitForCondition(condition);
}
