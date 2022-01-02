"use strict";

// Turn off startup scanning.
Services.prefs.setIntPref("extensions.startupScanScopes", 0);

createAppInfo("xpcshell@tessts.mozilla.org", "XPCShell", "42", "42");
// Prevent XPIStates.scanForChanges from seeing this as an update and forcing a
// full scan.
Services.prefs.setCharPref(
  "extensions.lastAppBuildId",
  Services.appinfo.appBuildID
);

// A small bootstrap calls monitor targeting a single extension (created to avoid introducing a workaround
// in BootstrapMonitor to be able to test Bug 1664144 fix).
let Monitor = {
  extensionId: undefined,
  collected: [],
  init() {
    const bootstrapCallListener = (_evtName, data) => {
      if (data.params.id == this.extensionId) {
        this.collected.push(data);
      }
    };
    AddonTestUtils.on("bootstrap-method", bootstrapCallListener);
    registerCleanupFunction(() => {
      AddonTestUtils.off("bootstrap-method", bootstrapCallListener);
    });
  },
  startCollecting(extensionId) {
    this.extensionId = extensionId;
  },
  stopCollecting() {
    this.extensionId = undefined;
  },
  getCollected() {
    const collected = this.collected;
    this.collected = [];
    return collected;
  },
};

Monitor.init();

// Bug 1664144: Test that during startup scans, updating an addon
// that has already started is restarted.
add_task(async function test_startup_sideload_updated() {
  const ID = "sideload@tests.mozilla.org";

  await createWebExtension(ID, initialVersion("1"), profileDir);
  await promiseStartupManager();

  // Ensure the sideload is enabled and running.
  let addon = await promiseAddonByID(ID);

  Monitor.startCollecting(ID);
  await addon.enable();
  Monitor.stopCollecting();

  let events = Monitor.getCollected();
  ok(events.length, "bootstrap methods called");
  equal(
    events[0].reason,
    BOOTSTRAP_REASONS.ADDON_ENABLE,
    "Startup reason is ADDON_ENABLE at install"
  );

  await promiseShutdownManager();
  // Touch the addon on disk before startup.
  await createWebExtension(ID, initialVersion("1.1"), profileDir);
  Monitor.startCollecting(ID);
  await promiseStartupManager();
  await AddonManagerPrivate.getNewSideloads();
  Monitor.stopCollecting();

  events = Monitor.getCollected().map(({ method, reason, params }) => {
    const { version } = params;
    return { method, reason, version };
  });

  const updatedVersion = "1.1.0";
  const expectedUpgradeParams = {
    reason: BOOTSTRAP_REASONS.ADDON_UPGRADE,
    version: updatedVersion,
  };

  const expectedCalls = [
    {
      method: "startup",
      reason: BOOTSTRAP_REASONS.APP_STARTUP,
      version: "1.0",
    },
    // Shutdown call has version 1.1 because the file was already
    // updated on disk and got the new version as part of the startup.
    { method: "shutdown", ...expectedUpgradeParams },
    { method: "update", ...expectedUpgradeParams },
    { method: "startup", ...expectedUpgradeParams },
  ];

  for (let i = 0; i < expectedCalls.length; i++) {
    Assert.deepEqual(
      events[i],
      expectedCalls[i],
      "Got the expected sequence of bootstrap method calls"
    );
  }

  equal(
    events.length,
    expectedCalls.length,
    "Got the expected number of bootstrap method calls"
  );

  // flush addonStartup.json
  await AddonTestUtils.loadAddonsList(true);
  // verify startupData is correct
  let startupData = aomStartup.readStartupData();
  Assert.equal(
    startupData["app-profile"].addons[ID].version,
    updatedVersion,
    "startup data is correct in cache"
  );

  await promiseShutdownManager();
});
