/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/
"use strict";

ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm", this);
ChromeUtils.import("resource://testing-common/ContentTaskUtils.jsm", this);

const Telemetry = Services.telemetry;
const TelemetryGeckoView = Cc["@mozilla.org/telemetry/geckoview-testing;1"]
                             .createInstance(Ci.nsITelemetryGeckoViewTesting);

/**
 * Run a file in the content process.
 * @param aFileName - The file to execute in the content process.
 * @return {Promise} A promise resolved after the execution in the other process
 *         finishes.
 */
async function run_in_child(aFileName) {
  const PREF_GECKOVIEW_MODE = "toolkit.telemetry.isGeckoViewMode";
  // We don't ship GeckoViewTelemetryController.jsm outside of Android. If
  // |toolkit.telemetry.isGeckoViewMode| is true, this makes Gecko crash on
  // other platforms because ContentProcessSingleton.js requires it. Work
  // around this by temporarily setting the pref to false.
  const currentValue = Services.prefs.getBoolPref(PREF_GECKOVIEW_MODE, false);
  Services.prefs.setBoolPref(PREF_GECKOVIEW_MODE, false);
  await run_test_in_child(aFileName);
  Services.prefs.setBoolPref(PREF_GECKOVIEW_MODE, currentValue);
}

/**
 * Builds a promise to wait for the GeckoView data loading to finish.
 * @return {Promise} A promise resolved when the data loading finishes.
 */
function waitGeckoViewLoadComplete() {
  return new Promise(resolve => {
    Services.obs.addObserver(function observe() {
      Services.obs.removeObserver(observe, "internal-telemetry-geckoview-load-complete");
      resolve();
    }, "internal-telemetry-geckoview-load-complete");
  });
}

/**
 * This function waits until the desired histogram is reported into the
 * snapshot of the relevant process.
 * @param aHistogramName - The name of the histogram to look for.
 * @param aProcessName - The name of the process to look in.
 * @param aKeyed - Whether or not to look in keyed snapshots.
 */
async function waitForHistogramSnapshotData(aHistogramName, aProcessName, aKeyed) {
  await ContentTaskUtils.waitForCondition(() => {
    const data = aKeyed
      ? Telemetry.snapshotKeyedHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false)
      : Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);

    return (aProcessName in data)
           && (aHistogramName in data[aProcessName]);
  });
}

/**
 * This function waits until the desired scalar is reported into the
 * snapshot of the relevant process.
 * @param aScalarName - The name of the scalar to look for.
 * @param aProcessName - The name of the process to look in.
 * @param aKeyed - Whether or not to look in keyed snapshots.
 */
async function waitForScalarSnapshotData(aScalarName, aProcessName, aKeyed) {
  await ContentTaskUtils.waitForCondition(() => {
    const data = aKeyed
      ? Telemetry.snapshotKeyedScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false)
      : Telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);

    return (aProcessName in data)
           && (aScalarName in data[aProcessName]);
  });
}

if (runningInParent) {
  Services.prefs.setBoolPref(TelemetryUtils.Preferences.OverridePreRelease, true);
}
