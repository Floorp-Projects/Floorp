"use strict";

const { ContentTaskUtils } = ChromeUtils.import("resource://testing-common/ContentTaskUtils.jsm");
const { TelemetryController } = ChromeUtils.import("resource://gre/modules/TelemetryController.jsm");
const { TelemetryUtils } = ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm");

const CONTENT_CREATED = "ipc:content-created";

async function waitForProcessesScalars(aProcesses, aKeyed,
                                       aAdditionalCondition = (data) => true) {
  await ContentTaskUtils.waitForCondition(() => {
    const scalars = aKeyed ?
      Services.telemetry.getSnapshotForKeyedScalars("main", false) :
      Services.telemetry.getSnapshotForScalars("main", false);
    return aProcesses.every(p => Object.keys(scalars).includes(p))
           && aAdditionalCondition(scalars);
  });
}

add_task(async function test_setup() {
  // Make sure the newly spawned content processes will have extended Telemetry enabled.
  await SpecialPowers.pushPrefEnv({
    set: [[TelemetryUtils.Preferences.OverridePreRelease, true]],
  });
  // And take care of the already initialized one as well.
  let canRecordExtended = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  registerCleanupFunction(() => Services.telemetry.canRecordExtended = canRecordExtended);
});

add_task(async function test_recording() {
  let currentPid = gBrowser.selectedBrowser.frameLoader.remoteTab.osPid;

  // Register test scalars before spawning the content process: the scalar
  // definitions will propagate to it.
  Services.telemetry.registerScalars("telemetry.test.dynamic", {
    "pre_content_spawn": {
      kind: Ci.nsITelemetry.SCALAR_TYPE_COUNT,
      keyed: false,
      record_on_release: true,
    },
    "pre_content_spawn_expiration": {
      kind: Ci.nsITelemetry.SCALAR_TYPE_COUNT,
      keyed: false,
      record_on_release: true,
    },
  });

  Services.telemetry.scalarSet("telemetry.test.dynamic.pre_content_spawn_expiration", 3);

  let processCreated = TestUtils.topicObserved(CONTENT_CREATED);
  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank", forceNewProcess: true },
                                    async function(browser) {
      // Make sure our new browser is in its own process. The processCreated
      // promise should have already resolved by this point.
      await processCreated;
      let newPid = browser.frameLoader.remoteTab.osPid;
      ok(currentPid != newPid, "The new tab must spawn its own process");

      // Register test scalars after spawning the content process: the scalar
      // definitions will propagate to it.
      // Also attempt to register again "pre_content_spawn_expiration" and set
      // it to expired.
      Services.telemetry.registerScalars("telemetry.test.dynamic", {
        "post_content_spawn": {
          kind: Ci.nsITelemetry.SCALAR_TYPE_BOOLEAN,
          keyed: false,
          record_on_release: false,
        },
        "post_content_spawn_keyed": {
          kind: Ci.nsITelemetry.SCALAR_TYPE_COUNT,
          keyed: true,
          record_on_release: true,
        },
        "pre_content_spawn_expiration": {
          kind: Ci.nsITelemetry.SCALAR_TYPE_COUNT,
          keyed: false,
          record_on_release: true,
          expired: true,
        },
      });

      // Accumulate from the content process into both dynamic scalars.
      await ContentTask.spawn(browser, {}, async function() {
        Services.telemetry.scalarAdd("telemetry.test.dynamic.pre_content_spawn_expiration", 1);
        Services.telemetry.scalarSet("telemetry.test.dynamic.pre_content_spawn", 3);
        Services.telemetry.scalarSet("telemetry.test.dynamic.post_content_spawn", true);
        Services.telemetry.keyedScalarSet("telemetry.test.dynamic.post_content_spawn_keyed",
                                          "testKey", 3);
      });
  });

  // Wait for the dynamic scalars to appear non-keyed snapshots.
  await waitForProcessesScalars(["dynamic"], false, scalars => {
    // Wait for the scalars set in the content process to be available.
    return "telemetry.test.dynamic.pre_content_spawn" in scalars.dynamic;
  });

  // Verify the content of the snapshots.
  const scalars =
      Services.telemetry.getSnapshotForScalars("main", false);
  ok("dynamic" in scalars,
     "The scalars must contain the 'dynamic' process section");
  ok("telemetry.test.dynamic.pre_content_spawn" in scalars.dynamic,
     "Dynamic scalars registered before a process spawns must be present.");
  is(scalars.dynamic["telemetry.test.dynamic.pre_content_spawn"], 3,
    "The dynamic scalar must contain the expected value.");
  is(scalars.dynamic["telemetry.test.dynamic.pre_content_spawn_expiration"], 3,
    "The dynamic scalar must not be updated after being expired.");
  ok("telemetry.test.dynamic.post_content_spawn" in scalars.dynamic,
     "Dynamic scalars registered after a process spawns must be present.");
  is(scalars.dynamic["telemetry.test.dynamic.post_content_spawn"], true,
     "The dynamic scalar must contain the expected value.");

  // Wait for the dynamic scalars to appear in the keyed snapshots.
  await waitForProcessesScalars(["dynamic"], true);

  const keyedScalars = Services.telemetry.getSnapshotForKeyedScalars("main", false);
  ok("dynamic" in keyedScalars,
     "The keyed scalars must contain the 'dynamic' process section");
  ok("telemetry.test.dynamic.post_content_spawn_keyed" in keyedScalars.dynamic,
     "Dynamic keyed scalars registered after a process spawns must be present.");
  is(keyedScalars.dynamic["telemetry.test.dynamic.post_content_spawn_keyed"].testKey, 3,
     "The dynamic keyed scalar must contain the expected value.");
});

add_task(async function test_aggregation() {
  Services.telemetry.clearScalars();

  // Register test scalars before spawning the content process: the scalar
  // definitions will propagate to it. Also cheat TelemetrySession to put
  // the test scalar in the payload by using "cheattest" instead of "test" in
  // the scalar category name.
  Services.telemetry.registerScalars("telemetry.cheattest.dynamic", {
    "test_aggregation": {
      kind: Ci.nsITelemetry.SCALAR_TYPE_COUNT,
      keyed: false,
      record_on_release: true,
    },
  });

  const SCALAR_FULL_NAME = "telemetry.cheattest.dynamic.test_aggregation";
  Services.telemetry.scalarAdd(SCALAR_FULL_NAME, 1);

  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank", forceNewProcess: true },
                                    async function(browser) {
      // Accumulate from the content process into both dynamic scalars.
      await ContentTask.spawn(browser, SCALAR_FULL_NAME, async function(aName) {
        Services.telemetry.scalarAdd(aName, 3);
      });
  });

  // Wait for the dynamic scalars to appear. Since we're testing that children
  // and parent data get aggregated, we might need to wait a bit more:
  // TelemetryIPCAccumulator.cpp sends batches to the parent process every 2 seconds.
  await waitForProcessesScalars(["dynamic"], false, scalarData => {
    return "dynamic" in scalarData
      && SCALAR_FULL_NAME in scalarData.dynamic
      && scalarData.dynamic[SCALAR_FULL_NAME] == 4;
  });

  // Check that the definitions made it to the ping payload.
  const pingData = TelemetryController.getCurrentPingData(true);
  ok("dynamic" in pingData.payload.processes,
     "The ping payload must contain the 'dynamic' process section");
  is(pingData.payload.processes.dynamic.scalars[SCALAR_FULL_NAME], 4,
    "The dynamic scalar must contain the aggregated parent and children data.");
});
