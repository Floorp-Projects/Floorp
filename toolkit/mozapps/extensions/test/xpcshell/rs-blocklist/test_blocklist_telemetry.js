/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "49"
);

const { TelemetryController } = ChromeUtils.import(
  "resource://gre/modules/TelemetryController.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

function assertTelemetryScalars(expectedScalars) {
  let scalars = TelemetryTestUtils.getProcessScalars("parent");

  for (const scalarName of Object.keys(expectedScalars || {})) {
    equal(
      scalars[scalarName],
      expectedScalars[scalarName],
      `Got the expected value for ${scalarName} scalar`
    );
  }
}

add_task(async function test_setup() {
  // Ensure that the telemetry scalar definitions are loaded and the
  // AddonManager initialized.
  await TelemetryController.testSetup();
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_blocklist_lastModified_rs_scalars() {
  const now = Date.now();

  const lastEntryTimes = {
    addons: now - 5000,
    addons_mlbf: now - 4000,
    plugins: now - 3000,
  };

  const lastEntryTimesUTC = {};
  const toUTC = t => new Date(t).toUTCString();
  for (const key of Object.keys(lastEntryTimes)) {
    lastEntryTimesUTC[key] = toUTC(lastEntryTimes[key]);
  }

  const {
    BlocklistTelemetry,
    ExtensionBlocklistRS,
    ExtensionBlocklistMLBF,
    PluginBlocklistRS,
  } = ChromeUtils.import("resource://gre/modules/Blocklist.jsm", null);

  // Return a promise resolved when the recordRSBlocklistLastModified method
  // has been called (by temporarily replacing the method with a function that
  // calls the real method and then resolve the promise).
  function promiseScalarRecorded() {
    return new Promise(resolve => {
      let origFn = BlocklistTelemetry.recordRSBlocklistLastModified;
      BlocklistTelemetry.recordRSBlocklistLastModified = async (...args) => {
        BlocklistTelemetry.recordRSBlocklistLastModified = origFn;
        let res = await origFn.apply(BlocklistTelemetry, args);
        resolve();
        return res;
      };
    });
  }

  async function fakeRemoteSettingsSync(rsClient, lastModified) {
    await rsClient.db.importChanges({}, lastModified);
    await rsClient.emit("sync");
  }

  info("Test RS plugins blocklist lastModified scalar");

  await PluginBlocklistRS.ensureInitialized();
  await Promise.all([
    promiseScalarRecorded(),
    fakeRemoteSettingsSync(PluginBlocklistRS._client, lastEntryTimes.plugins),
  ]);

  assertTelemetryScalars({
    "blocklist.lastModified_rs_addons": undefined,
    "blocklist.lastModified_rs_addons_mlbf": undefined,
    "blocklist.lastModified_rs_plugins": lastEntryTimesUTC.plugins,
  });

  info("Test RS addon blocklist lastModified scalar");

  await ExtensionBlocklistRS.ensureInitialized();
  await Promise.all([
    promiseScalarRecorded(),
    fakeRemoteSettingsSync(ExtensionBlocklistRS._client, lastEntryTimes.addons),
  ]);

  assertTelemetryScalars({
    "blocklist.lastModified_rs_addons": lastEntryTimesUTC.addons,
    "blocklist.lastModified_rs_addons_mlbf": undefined,
    "blocklist.lastModified_rs_plugins": lastEntryTimesUTC.plugins,
  });

  await ExtensionBlocklistMLBF.ensureInitialized();
  await Promise.all([
    promiseScalarRecorded(),
    fakeRemoteSettingsSync(
      ExtensionBlocklistMLBF._client,
      lastEntryTimes.addons_mlbf
    ),
  ]);

  assertTelemetryScalars({
    "blocklist.lastModified_rs_addons": lastEntryTimesUTC.addons,
    "blocklist.lastModified_rs_addons_mlbf": lastEntryTimesUTC.addons_mlbf,
    "blocklist.lastModified_rs_plugins": lastEntryTimesUTC.plugins,
  });
});
