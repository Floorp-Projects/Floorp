// Test that the expected installTelemetryInfo are being set on the system addon
// installed/updated through Balrog.

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2");

let distroDir = FileUtils.getDir("ProfD", ["sysfeatures", "empty"], true);
registerDirectory("XREAppFeat", distroDir);

AddonTestUtils.usePrivilegedSignatures = id => "system";

add_task(() => initSystemAddonDirs());

add_task(async function test_addon_update() {
  Services.prefs.setBoolPref(PREF_SYSTEM_ADDON_UPDATE_ENABLED, true);

  // Define the set of initial conditions to run each test against:
  // - setup:        A task to setup the profile into the initial state.
  // - initialState: The initial expected system add-on state after setup has run.
  const initialSetup = {
    async setup() {
      await buildPrefilledUpdatesDir();
      distroDir.leafName = "empty";
    },
    initialState: [
      { isUpgrade: false, version: null },
      { isUpgrade: true, version: "2.0" },
    ],
  };

  await setupSystemAddonConditions(initialSetup, distroDir);

  const newlyInstalledSystemAddonId = "system1@tests.mozilla.org";
  const updatedSystemAddonId = "system2@tests.mozilla.org";

  const updateXML = buildSystemAddonUpdates([
    // Newly installed system addon entry.
    {
      id: newlyInstalledSystemAddonId,
      version: "1.0",
      path: "system1_1.xpi",
      xpi: await getSystemAddonXPI(1, "1.0"),
    },
    // Updated system addon entry.
    {
      id: updatedSystemAddonId,
      version: "3.0",
      path: "system2_3.xpi",
      xpi: await getSystemAddonXPI(2, "3.0"),
    },
  ]);

  await Promise.all([
    updateAllSystemAddons(updateXML),
    promiseWebExtensionStartup(newlyInstalledSystemAddonId),
    promiseWebExtensionStartup(updatedSystemAddonId),
  ]);

  await verifySystemAddonState(
    initialSetup.initialState,
    [
      { isUpgrade: true, version: "1.0" },
      { isUpgrade: true, version: "3.0" },
      { isUpgrade: false, version: null },
      { isUpgrade: false, version: null },
      { isUpgrade: false, version: null },
    ],
    false,
    distroDir
  );

  const newlyInstalledSystemAddon = await AddonManager.getAddonByID(
    newlyInstalledSystemAddonId
  );
  Assert.deepEqual(
    newlyInstalledSystemAddon.installTelemetryInfo,
    // For addons installed for the first time through the product addon checker
    // we set a `method` in the telemetryInfo.
    { source: "system-addon", method: "product-updates" },
    "Got the expected telemetry info on balrog system addon installed addon"
  );

  const updatedSystemAddon = await AddonManager.getAddonByID(
    updatedSystemAddonId
  );
  Assert.deepEqual(
    updatedSystemAddon.installTelemetryInfo,
    // For addons that are distributed in Firefox, then updated through the product
    // addon checker, `method` will not be set.
    { source: "system-addon" },
    "Got the expected telemetry info on balrog system addon updated addon"
  );

  await promiseShutdownManager();
});
