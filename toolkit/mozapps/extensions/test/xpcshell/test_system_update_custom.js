// Tests that system add-on upgrades work.

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2");

let distroDir = FileUtils.getDir("ProfD", ["sysfeatures", "empty"], true);
registerDirectory("XREAppFeat", distroDir);

AddonTestUtils.usePrivilegedSignatures = id => "system";

add_task(() => initSystemAddonDirs());

/**
 * Defines the set of initial conditions to run each test against. Each should
 * define the following properties:
 *
 * setup:        A task to setup the profile into the initial state.
 * initialState: The initial expected system add-on state after setup has run.
 */
const TEST_CONDITIONS = {
  // Runs tests with no updated or default system add-ons initially installed
  blank: {
    setup() {
      clearSystemAddonUpdatesDir();
      distroDir.leafName = "empty";
    },
    initialState: [
      { isUpgrade: false, version: null },
      { isUpgrade: false, version: null },
      { isUpgrade: false, version: null },
      { isUpgrade: false, version: null },
      { isUpgrade: false, version: null },
    ],
  },
  // Runs tests with default system add-ons installed
  withAppSet: {
    setup() {
      clearSystemAddonUpdatesDir();
      distroDir.leafName = "prefilled";
    },
    initialState: [
      { isUpgrade: false, version: null },
      { isUpgrade: false, version: "2.0" },
      { isUpgrade: false, version: "2.0" },
      { isUpgrade: false, version: null },
      { isUpgrade: false, version: null },
    ],
  },

  // Runs tests with updated system add-ons installed
  withProfileSet: {
    async setup() {
      await buildPrefilledUpdatesDir();
      distroDir.leafName = "empty";
    },
    initialState: [
      { isUpgrade: false, version: null },
      { isUpgrade: true, version: "2.0" },
      { isUpgrade: true, version: "2.0" },
      { isUpgrade: false, version: null },
      { isUpgrade: false, version: null },
    ],
  },

  // Runs tests with both default and updated system add-ons installed
  withBothSets: {
    async setup() {
      await buildPrefilledUpdatesDir();
      distroDir.leafName = "hidden";
    },
    initialState: [
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: true, version: "2.0" },
      { isUpgrade: true, version: "2.0" },
      { isUpgrade: false, version: null },
      { isUpgrade: false, version: null },
    ],
  },
};

// Test that the update check is performed as part of the regular add-on update
// check
add_task(async function test_addon_update() {
  Services.prefs.setBoolPref(PREF_SYSTEM_ADDON_UPDATE_ENABLED, true);
  await setupSystemAddonConditions(TEST_CONDITIONS.blank, distroDir);

  let updateXML = buildSystemAddonUpdates([
    {
      id: "system2@tests.mozilla.org",
      version: "2.0",
      path: "system2_2.xpi",
      xpi: await getSystemAddonXPI(2, "2.0"),
    },
    {
      id: "system3@tests.mozilla.org",
      version: "2.0",
      path: "system3_2.xpi",
      xpi: await getSystemAddonXPI(3, "2.0"),
    },
  ]);
  await Promise.all([
    updateAllSystemAddons(updateXML),
    promiseWebExtensionStartup("system2@tests.mozilla.org"),
    promiseWebExtensionStartup("system3@tests.mozilla.org"),
  ]);

  await verifySystemAddonState(
    TEST_CONDITIONS.blank.initialState,
    [
      { isUpgrade: false, version: null },
      { isUpgrade: true, version: "2.0" },
      { isUpgrade: true, version: "2.0" },
      { isUpgrade: false, version: null },
      { isUpgrade: false, version: null },
    ],
    false,
    distroDir
  );

  await promiseShutdownManager();
});

// Disabling app updates should block system add-on updates
add_task(async function test_app_update_disabled() {
  await setupSystemAddonConditions(TEST_CONDITIONS.blank, distroDir);

  Services.prefs.setBoolPref(PREF_SYSTEM_ADDON_UPDATE_ENABLED, false);
  let updateXML = buildSystemAddonUpdates([
    {
      id: "system2@tests.mozilla.org",
      version: "2.0",
      path: "system2_2.xpi",
      xpi: await getSystemAddonXPI(2, "2.0"),
    },
    {
      id: "system3@tests.mozilla.org",
      version: "2.0",
      path: "system3_2.xpi",
      xpi: await getSystemAddonXPI(3, "2.0"),
    },
  ]);
  await updateAllSystemAddons(updateXML);
  Services.prefs.clearUserPref(PREF_SYSTEM_ADDON_UPDATE_ENABLED);

  await verifySystemAddonState(
    TEST_CONDITIONS.blank.initialState,
    undefined,
    false,
    distroDir
  );

  await promiseShutdownManager();
});

// Safe mode should block system add-on updates
add_task(async function test_safe_mode() {
  gAppInfo.inSafeMode = true;

  await setupSystemAddonConditions(TEST_CONDITIONS.blank, distroDir);

  Services.prefs.setBoolPref(PREF_SYSTEM_ADDON_UPDATE_ENABLED, false);
  let updateXML = buildSystemAddonUpdates([
    {
      id: "system2@tests.mozilla.org",
      version: "2.0",
      path: "system2_2.xpi",
      xpi: await getSystemAddonXPI(2, "2.0"),
    },
    {
      id: "system3@tests.mozilla.org",
      version: "2.0",
      path: "system3_2.xpi",
      xpi: await getSystemAddonXPI(3, "2.0"),
    },
  ]);
  await updateAllSystemAddons(updateXML);
  Services.prefs.clearUserPref(PREF_SYSTEM_ADDON_UPDATE_ENABLED);

  await verifySystemAddonState(
    TEST_CONDITIONS.blank.initialState,
    undefined,
    false,
    distroDir
  );

  await promiseShutdownManager();

  gAppInfo.inSafeMode = false;
});

// Tests that a set that matches the default set does nothing
add_task(async function test_match_default() {
  await setupSystemAddonConditions(TEST_CONDITIONS.withAppSet, distroDir);

  let installXML = buildSystemAddonUpdates([
    {
      id: "system2@tests.mozilla.org",
      version: "2.0",
      path: "system2_2.xpi",
      xpi: await getSystemAddonXPI(2, "2.0"),
    },
    {
      id: "system3@tests.mozilla.org",
      version: "2.0",
      path: "system3_2.xpi",
      xpi: await getSystemAddonXPI(3, "2.0"),
    },
  ]);
  await installSystemAddons(installXML);

  // Shouldn't have installed an updated set
  await verifySystemAddonState(
    TEST_CONDITIONS.withAppSet.initialState,
    undefined,
    false,
    distroDir
  );

  await promiseShutdownManager();
});

// Tests that a set that matches the hidden default set works
add_task(async function test_match_default_revert() {
  await setupSystemAddonConditions(TEST_CONDITIONS.withBothSets, distroDir);

  let installXML = buildSystemAddonUpdates([
    {
      id: "system1@tests.mozilla.org",
      version: "1.0",
      path: "system1_1.xpi",
      xpi: await getSystemAddonXPI(1, "1.0"),
    },
    {
      id: "system2@tests.mozilla.org",
      version: "1.0",
      path: "system2_1.xpi",
      xpi: await getSystemAddonXPI(2, "1.0"),
    },
  ]);
  await installSystemAddons(installXML);

  // This should revert to the default set instead of installing new versions
  // into an updated set.
  await verifySystemAddonState(
    TEST_CONDITIONS.withBothSets.initialState,
    [
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: false, version: null },
      { isUpgrade: false, version: null },
      { isUpgrade: false, version: null },
    ],
    false,
    distroDir
  );

  await promiseShutdownManager();
});

// Tests that a set that matches the current set works
add_task(async function test_match_current() {
  await setupSystemAddonConditions(TEST_CONDITIONS.withBothSets, distroDir);

  let installXML = buildSystemAddonUpdates([
    {
      id: "system2@tests.mozilla.org",
      version: "2.0",
      path: "system2_2.xpi",
      xpi: await getSystemAddonXPI(2, "2.0"),
    },
    {
      id: "system3@tests.mozilla.org",
      version: "2.0",
      path: "system3_2.xpi",
      xpi: await getSystemAddonXPI(3, "2.0"),
    },
  ]);
  await installSystemAddons(installXML);

  // This should remain with the current set instead of creating a new copy
  let set = JSON.parse(Services.prefs.getCharPref(PREF_SYSTEM_ADDON_SET));
  Assert.equal(set.directory, "prefilled");

  await verifySystemAddonState(
    TEST_CONDITIONS.withBothSets.initialState,
    undefined,
    false,
    distroDir
  );

  await promiseShutdownManager();
});

// Tests that a set with a minor change doesn't re-download existing files
add_task(async function test_no_download() {
  await setupSystemAddonConditions(TEST_CONDITIONS.withBothSets, distroDir);

  // The missing file here is unneeded since there is a local version already
  let installXML = buildSystemAddonUpdates([
    { id: "system2@tests.mozilla.org", version: "2.0", path: "missing.xpi" },
    {
      id: "system4@tests.mozilla.org",
      version: "1.0",
      path: "system4_1.xpi",
      xpi: await getSystemAddonXPI(4, "1.0"),
    },
  ]);
  await Promise.all([
    installSystemAddons(installXML),
    promiseWebExtensionStartup("system4@tests.mozilla.org"),
  ]);

  await verifySystemAddonState(
    TEST_CONDITIONS.withBothSets.initialState,
    [
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: true, version: "2.0" },
      { isUpgrade: false, version: null },
      { isUpgrade: true, version: "1.0" },
      { isUpgrade: false, version: null },
    ],
    false,
    distroDir
  );

  await promiseShutdownManager();
});

// Tests that a second update before a restart works
add_task(async function test_double_update() {
  await setupSystemAddonConditions(TEST_CONDITIONS.withAppSet, distroDir);

  let installXML = buildSystemAddonUpdates([
    {
      id: "system2@tests.mozilla.org",
      version: "2.0",
      path: "system2_2.xpi",
      xpi: await getSystemAddonXPI(2, "2.0"),
    },
    {
      id: "system3@tests.mozilla.org",
      version: "1.0",
      path: "system3_1.xpi",
      xpi: await getSystemAddonXPI(3, "1.0"),
    },
  ]);
  await Promise.all([
    installSystemAddons(installXML),
    promiseWebExtensionStartup("system2@tests.mozilla.org"),
    promiseWebExtensionStartup("system3@tests.mozilla.org"),
  ]);

  installXML = buildSystemAddonUpdates([
    {
      id: "system3@tests.mozilla.org",
      version: "2.0",
      path: "system3_2.xpi",
      xpi: await getSystemAddonXPI(3, "2.0"),
    },
    {
      id: "system4@tests.mozilla.org",
      version: "1.0",
      path: "system4_1.xpi",
      xpi: await getSystemAddonXPI(4, "1.0"),
    },
  ]);
  await Promise.all([
    installSystemAddons(installXML),
    promiseWebExtensionStartup("system3@tests.mozilla.org"),
    promiseWebExtensionStartup("system4@tests.mozilla.org"),
  ]);

  await verifySystemAddonState(
    TEST_CONDITIONS.withAppSet.initialState,
    [
      { isUpgrade: false, version: null },
      { isUpgrade: false, version: "2.0" },
      { isUpgrade: true, version: "2.0" },
      { isUpgrade: true, version: "1.0" },
      { isUpgrade: false, version: null },
    ],
    true,
    distroDir
  );

  await promiseShutdownManager();
});

// A second update after a restart will delete the original unused set
add_task(async function test_update_purges() {
  await setupSystemAddonConditions(TEST_CONDITIONS.withBothSets, distroDir);

  let installXML = buildSystemAddonUpdates([
    {
      id: "system2@tests.mozilla.org",
      version: "2.0",
      path: "system2_2.xpi",
      xpi: await getSystemAddonXPI(2, "2.0"),
    },
    {
      id: "system3@tests.mozilla.org",
      version: "1.0",
      path: "system3_1.xpi",
      xpi: await getSystemAddonXPI(3, "1.0"),
    },
  ]);
  await Promise.all([
    installSystemAddons(installXML),
    promiseWebExtensionStartup("system2@tests.mozilla.org"),
    promiseWebExtensionStartup("system3@tests.mozilla.org"),
  ]);

  await verifySystemAddonState(
    TEST_CONDITIONS.withBothSets.initialState,
    [
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: true, version: "2.0" },
      { isUpgrade: true, version: "1.0" },
      { isUpgrade: false, version: null },
      { isUpgrade: false, version: null },
    ],
    false,
    distroDir
  );

  await installSystemAddons(buildSystemAddonUpdates(null));

  let dirs = await getSystemAddonDirectories();
  Assert.equal(dirs.length, 1);

  await promiseShutdownManager();
});
