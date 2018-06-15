/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/
"use strict";

ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Services.jsm", this);
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
async function waitForSnapshotData(aHistogramName, aProcessName, aKeyed) {
  await ContentTaskUtils.waitForCondition(() => {
    const data = aKeyed
      ? Telemetry.snapshotKeyedHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false)
      : Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);

    return (aProcessName in data)
           && (aHistogramName in data[aProcessName]);
  });
}

add_task(async function setup() {
  // Init the profile.
  let profileDir = do_get_profile(true);

  // Set geckoview mode.
  Services.prefs.setBoolPref("toolkit.telemetry.isGeckoViewMode", true);

  // Set the ANDROID_DATA_DIR to the profile dir.
  let env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  env.set("MOZ_ANDROID_DATA_DIR", profileDir.path);
});

add_task(async function test_persistContentHistograms() {
  // Get and clear the histograms.
  let plainHist = Telemetry.getHistogramById("TELEMETRY_TEST_MULTIPRODUCT");
  plainHist.clear();
  let keyedHist = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_COUNT");
  keyedHist.clear();

  TelemetryGeckoView.initPersistence();

  // Set the histograms in parent.
  plainHist.add(37);
  keyedHist.add("parent-test-key", 73);

  // Set content histograms and wait for the execution in the other
  // process to finish.
  await run_in_child("test_GeckoView_content_histograms.js");

  // Wait for the data to be collected by the parent process.
  await waitForSnapshotData("TELEMETRY_TEST_MULTIPRODUCT", "content", false /* aKeyed */);
  await waitForSnapshotData("TELEMETRY_TEST_KEYED_COUNT", "content", true /* aKeyed */);

  // Force persisting the measurements to file.
  TelemetryGeckoView.forcePersist();
  TelemetryGeckoView.deInitPersistence();

  // Clear the histograms for all processes.
  Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true /* clear */);
  Telemetry.snapshotKeyedHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true /* clear */);

  // Start the persistence system again, to unpersist the data.
  let loadPromise = waitGeckoViewLoadComplete();
  TelemetryGeckoView.initPersistence();
  // Wait for the load to finish.
  await loadPromise;

  // Validate the snapshot data.
  const snapshot =
    Telemetry.snapshotHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false /* clear */);
  Assert.ok("parent" in snapshot, "The snapshot object must have a 'parent' entry.");
  Assert.ok("content" in snapshot, "The snapshot object must have a 'content' entry.");
  Assert.ok("TELEMETRY_TEST_MULTIPRODUCT" in snapshot.parent,
            "The TELEMETRY_TEST_MULTIPRODUCT histogram must exist in the parent section.");
  Assert.equal(snapshot.parent.TELEMETRY_TEST_MULTIPRODUCT.sum, 37,
               "The TELEMETRY_TEST_MULTIPRODUCT must have the expected value in the parent section.");
  Assert.ok("TELEMETRY_TEST_MULTIPRODUCT" in snapshot.content,
            "The TELEMETRY_TEST_MULTIPRODUCT histogram must exist in the content section.");
  Assert.equal(snapshot.content.TELEMETRY_TEST_MULTIPRODUCT.sum, 73,
               "The TELEMETRY_TEST_MULTIPRODUCT must have the expected value in the content section.");

  const keyedSnapshot =
    Telemetry.snapshotKeyedHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false /* clear */);
  Assert.ok("parent" in keyedSnapshot, "The keyed snapshot object must have a 'parent' entry.");
  Assert.ok("content" in keyedSnapshot, "The keyed snapshot object must have a 'content' entry.");
  const parentData = keyedSnapshot.parent;
  Assert.ok("TELEMETRY_TEST_KEYED_COUNT" in parentData,
            "The TELEMETRY_TEST_KEYED_COUNT histogram must exist in the parent section.");
  Assert.ok("parent-test-key" in parentData.TELEMETRY_TEST_KEYED_COUNT,
            "The histogram in the parent process should have the expected key.");
  Assert.equal(parentData.TELEMETRY_TEST_KEYED_COUNT["parent-test-key"].sum, 73,
               "The TELEMETRY_TEST_KEYED_COUNT must have the expected value in the parent section.");
  const contentData = keyedSnapshot.content;
  Assert.ok("TELEMETRY_TEST_KEYED_COUNT" in contentData,
            "The TELEMETRY_TEST_KEYED_COUNT histogram must exist in the content section.");
  Assert.ok("content-test-key" in contentData.TELEMETRY_TEST_KEYED_COUNT,
            "The histogram in the content process should have the expected key.");
  Assert.equal(contentData.TELEMETRY_TEST_KEYED_COUNT["content-test-key"].sum, 37,
               "The TELEMETRY_TEST_KEYED_COUNT must have the expected value in the content section.");

  TelemetryGeckoView.deInitPersistence();
});

add_task(async function cleanup() {
  Services.prefs.clearUserPref("toolkit.telemetry.isGeckoViewMode");
});
