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

const { TelemetryController } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryController.sys.mjs"
);
const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

add_setup({ skip_if: () => IS_ANDROID_BUILD }, function test_setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();
});

function assertTelemetryScalars(expectedScalars) {
  if (!IS_ANDROID_BUILD) {
    let scalars = TelemetryTestUtils.getProcessScalars("parent");

    for (const scalarName of Object.keys(expectedScalars || {})) {
      equal(
        scalars[scalarName],
        expectedScalars[scalarName],
        `Got the expected value for ${scalarName} scalar`
      );
    }
  } else {
    info(
      `Skip assertions on collected samples for ${expectedScalars} on android builds`
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
  Services.fog.testResetFOG();
  const now = Date.now();

  const lastEntryTimes = {
    addons: now - 5000,
    addons_mlbf: now - 4000,
  };

  const lastEntryTimesUTC = {};
  const toUTC = t => new Date(t).toUTCString();
  for (const key of Object.keys(lastEntryTimes)) {
    lastEntryTimesUTC[key] = toUTC(lastEntryTimes[key]);
  }

  const {
    BlocklistPrivate: {
      BlocklistTelemetry,
      ExtensionBlocklistMLBF,
      ExtensionBlocklistRS,
    },
  } = ChromeUtils.importESModule("resource://gre/modules/Blocklist.sys.mjs");

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

  assertTelemetryScalars({
    "blocklist.lastModified_rs_addons_mlbf": undefined,
  });
  Assert.equal(
    undefined,
    Glean.blocklist.lastModifiedRsAddonsMblf.testGetValue()
  );

  info("Test RS addon blocklist lastModified scalar");

  await ExtensionBlocklistRS.ensureInitialized();
  await Promise.all([
    promiseScalarRecorded(),
    fakeRemoteSettingsSync(ExtensionBlocklistRS._client, lastEntryTimes.addons),
  ]);

  assertTelemetryScalars({
    "blocklist.lastModified_rs_addons_mlbf": undefined,
  });

  Assert.equal(
    undefined,
    Glean.blocklist.lastModifiedRsAddonsMblf.testGetValue()
  );

  await ExtensionBlocklistMLBF.ensureInitialized();
  await Promise.all([
    promiseScalarRecorded(),
    fakeRemoteSettingsSync(
      ExtensionBlocklistMLBF._client,
      lastEntryTimes.addons_mlbf
    ),
  ]);

  assertTelemetryScalars({
    "blocklist.lastModified_rs_addons_mlbf": lastEntryTimesUTC.addons_mlbf,
  });
  Assert.equal(
    new Date(lastEntryTimesUTC.addons_mlbf).getTime(),
    Glean.blocklist.lastModifiedRsAddonsMblf.testGetValue().getTime()
  );
});
