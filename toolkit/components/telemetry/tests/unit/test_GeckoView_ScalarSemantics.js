/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/
"use strict";

ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://testing-common/ContentTaskUtils.jsm", this);

const CONTENT_ONLY_UINT_SCALAR = "telemetry.test.content_only_uint";
const ALL_PROCESSES_UINT_SCALAR = "telemetry.test.all_processes_uint";
const DEFAULT_PRODUCTS_SCALAR = "telemetry.test.default_products";
const CHILD_KEYED_UNSIGNED_INT = "telemetry.test.child_keyed_unsigned_int";

add_task(async function setup() {
  // Init the profile.
  let profileDir = do_get_profile(true);

  // Set geckoview mode.
  Services.prefs.setBoolPref("toolkit.telemetry.isGeckoViewMode", true);

  // Set the ANDROID_DATA_DIR to the profile dir.
  let env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  env.set("MOZ_ANDROID_DATA_DIR", profileDir.path);
});

add_task(async function test_MultiprocessScalarSemantics() {
  /**
   * To mitigate races during deserialization of persisted data
   * scalar operations will be recorded and applied after the deserialization is finished.
   *
   * This test ensures it works acording to the semantics and follows the documentation example:
   *
   *  * Scalar deserialization is started
   *  * “test” scalar is incremented by “10” by the application -> The operation [test, add, 10] is recorded into the list.
   *  * The state of the “test” scalar is loaded off the persistence file, and the value “14” is set.
   *  * Deserialization is finished and the pending operations are applied.
   *  * The “test” scalar is incremented by “10”, the value is now “24”
   */

  Telemetry.clearScalars();

  let loadPromise = waitGeckoViewLoadComplete();
  TelemetryGeckoView.initPersistence();
  await loadPromise;

  // Set something in the parent
  Telemetry.scalarSet(ALL_PROCESSES_UINT_SCALAR, 34);

  // Set scalars in the child process.
  // The child will then wait for a signal to continue.
  let child = run_in_child("test_GeckoView_content_scalars.js");

  // Wait for the data to be collected by the parent process.
  await waitForScalarSnapshotData(ALL_PROCESSES_UINT_SCALAR, "parent", false /* aKeyed */);
  await waitForScalarSnapshotData(CONTENT_ONLY_UINT_SCALAR, "content", false /* aKeyed */);
  await waitForScalarSnapshotData(ALL_PROCESSES_UINT_SCALAR, "content", false /* aKeyed */);
  await waitForScalarSnapshotData(CHILD_KEYED_UNSIGNED_INT, "content", true /* aKeyed */);

  let snapshot = Telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false /* clear */);
  let keyedSnapshot = Telemetry.snapshotKeyedScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false /* clear */);
  Assert.equal(snapshot.content[CONTENT_ONLY_UINT_SCALAR], 14,
            `The ${CONTENT_ONLY_UINT_SCALAR} scalar must have the expected value in the content section.`);
  Assert.equal(snapshot.content[ALL_PROCESSES_UINT_SCALAR], 24,
            `The ${ALL_PROCESSES_UINT_SCALAR} scalar must have the expected value in the content section.`);
  Assert.equal(snapshot.parent[ALL_PROCESSES_UINT_SCALAR], 34,
            `The ${ALL_PROCESSES_UINT_SCALAR} scalar must have the expected value in the parent section.`);
  Assert.equal(keyedSnapshot.content[CHILD_KEYED_UNSIGNED_INT].chewbacca, 44,
            `The ${CHILD_KEYED_UNSIGNED_INT} keyed scalar must have the expected value in the content section.`);

  // Force persisting the measurements to file.
  TelemetryGeckoView.forcePersist();
  TelemetryGeckoView.deInitPersistence();

  // Clear all data from memory.
  Telemetry.clearScalars();

  // Mark deserialization as in progress, following operations are recorded and not applied.
  TelemetryGeckoView.deserializationStarted();

  // Modify a scalar in the parent process.
  Telemetry.scalarAdd(ALL_PROCESSES_UINT_SCALAR, 10);

  // Let child know to progress and wait for it to finish.
  do_send_remote_message("child-scalar-semantics");
  await child;

  // Start the persistence system again, to unpersist the data.
  loadPromise = waitGeckoViewLoadComplete();
  TelemetryGeckoView.initPersistence();
  // Wait for the load to finish.
  await loadPromise;

  // Wait for the data to be collected by the parent process.
  // We only wait for the new data, as the rest will be in there from the persistence load.
  await waitForScalarSnapshotData(DEFAULT_PRODUCTS_SCALAR, "content", false /* aKeyed */);

  // Validate the snapshot data.
  snapshot =
    Telemetry.snapshotScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false /* clear */);
  keyedSnapshot =
    Telemetry.snapshotKeyedScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false /* clear */);

  Assert.ok("parent" in snapshot, "The snapshot object must have a 'content' entry.");
  Assert.ok("content" in snapshot, "The snapshot object must have a 'content' entry.");
  Assert.ok("content" in keyedSnapshot, "The keyed snapshot object must have a 'content' entry.");

  Assert.ok(ALL_PROCESSES_UINT_SCALAR in snapshot.parent,
            `The ${ALL_PROCESSES_UINT_SCALAR} scalar must exist in the parent section.`);
  Assert.ok(CONTENT_ONLY_UINT_SCALAR in snapshot.content,
            `The ${CONTENT_ONLY_UINT_SCALAR} scalar must exist in the content section.`);
  Assert.ok(ALL_PROCESSES_UINT_SCALAR in snapshot.content,
            `The ${ALL_PROCESSES_UINT_SCALAR} scalar must exist in the content section.`);

  Assert.equal(snapshot.content[CONTENT_ONLY_UINT_SCALAR], 24,
               `The ${CONTENT_ONLY_UINT_SCALAR} must have the expected value in the content section.`);
  Assert.equal(snapshot.content[ALL_PROCESSES_UINT_SCALAR], 34,
               `The ${ALL_PROCESSES_UINT_SCALAR} must have the expected value in the content section.`);

  Assert.equal(snapshot.parent[ALL_PROCESSES_UINT_SCALAR], 44,
               `The ${ALL_PROCESSES_UINT_SCALAR} must have the expected value in the parent section.`);

  Assert.equal(keyedSnapshot.content[CHILD_KEYED_UNSIGNED_INT].chewbacca, 54,
            `The ${CHILD_KEYED_UNSIGNED_INT} keyed scalar must have the expected value in the content section.`);

  TelemetryGeckoView.deInitPersistence();
});

add_task(async function cleanup() {
  Services.prefs.clearUserPref("toolkit.telemetry.isGeckoViewMode");
});
