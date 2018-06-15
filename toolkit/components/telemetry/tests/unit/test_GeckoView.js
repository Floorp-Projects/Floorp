/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/
"use strict";

ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://testing-common/ContentTaskUtils.jsm", this);

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
  await waitForHistogramSnapshotData("TELEMETRY_TEST_MULTIPRODUCT", "content", false /* aKeyed */);
  await waitForHistogramSnapshotData("TELEMETRY_TEST_KEYED_COUNT", "content", true /* aKeyed */);

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
