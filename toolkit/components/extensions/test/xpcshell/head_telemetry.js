/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* exported IS_OOP, arraySum, clearHistograms, getSnapshots, promiseTelemetryRecorded */

ChromeUtils.defineModuleGetter(this, "ContentTaskUtils",
                               "resource://testing-common/ContentTaskUtils.jsm");

const IS_OOP = Services.prefs.getBoolPref("extensions.webextensions.remote");

function arraySum(arr) {
  return arr.reduce((a, b) => a + b, 0);
}

function clearHistograms() {
  Services.telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                        true /* clear */);
}

function getSnapshots(process) {
  return Services.telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                               false /* clear */)[process];
}

// TODO Bug 1357509: There is no good way to make sure that the parent received
// the histogram entries from the extension and content processes.  Let's stick
// to the ugly, spinning the event loop until we have a good approach.
function promiseTelemetryRecorded(id, process, expectedCount) {
  let condition = () => {
    let snapshot = Services.telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN,
                                                         false /* clear */)[process][id];
    return snapshot && arraySum(snapshot.counts) >= expectedCount;
  };
  return ContentTaskUtils.waitForCondition(condition);
}
