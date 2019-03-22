// Tests that we reset to the default system add-ons correctly when switching
// application versions

const updatesDir = FileUtils.getDir("ProfD", ["features"]);

AddonTestUtils.usePrivilegedSignatures = id => "system";

add_task(async function setup() {
  // Build the test sets
  let dir = FileUtils.getDir("ProfD", ["sysfeatures", "app1"], true);
  let xpi = await getSystemAddonXPI(1, "1.0");
  xpi.copyTo(dir, "system1@tests.mozilla.org.xpi");

  xpi = await getSystemAddonXPI(2, "1.0");
  xpi.copyTo(dir, "system2@tests.mozilla.org.xpi");

  dir = FileUtils.getDir("ProfD", ["sysfeatures", "app2"], true);
  xpi = await getSystemAddonXPI(1, "2.0");
  xpi.copyTo(dir, "system1@tests.mozilla.org.xpi");

  xpi = await getSystemAddonXPI(3, "1.0");
  xpi.copyTo(dir, "system3@tests.mozilla.org.xpi");

  dir = FileUtils.getDir("ProfD", ["sysfeatures", "app3"], true);
  xpi = await getSystemAddonXPI(1, "1.0");
  xpi.copyTo(dir, "system1@tests.mozilla.org.xpi");

  xpi = await getSystemAddonXPI(3, "1.0");
  xpi.copyTo(dir, "system3@tests.mozilla.org.xpi");
});

const distroDir = FileUtils.getDir("ProfD", ["sysfeatures", "app0"], true);
registerDirectory("XREAppFeat", distroDir);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "0");

function makeUUID() {
  let uuidGen = Cc["@mozilla.org/uuid-generator;1"].
                getService(Ci.nsIUUIDGenerator);
  return uuidGen.generateUUID().toString();
}

async function check_installed(conditions) {
  for (let i = 0; i < conditions.length; i++) {
    let condition = conditions[i];
    let id = "system" + (i + 1) + "@tests.mozilla.org";
    let addon = await promiseAddonByID(id);

    if (!("isUpgrade" in condition) || !("version" in condition)) {
      throw Error("condition must contain isUpgrade and version");
    }
    let isUpgrade = conditions[i].isUpgrade;
    let version = conditions[i].version;

    let expectedDir = isUpgrade ? updatesDir : distroDir;

    if (version) {
      // Add-on should be installed
      Assert.notEqual(addon, null);
      Assert.equal(addon.version, version);
      Assert.ok(addon.isActive);
      Assert.ok(!addon.foreignInstall);
      Assert.ok(addon.hidden);
      Assert.ok(addon.isSystem);
      Assert.ok(!hasFlag(addon.permissions, AddonManager.PERM_CAN_UPGRADE));
      if (isUpgrade) {
        Assert.ok(hasFlag(addon.permissions, AddonManager.PERM_API_CAN_UNINSTALL));
      } else {
        Assert.ok(!hasFlag(addon.permissions, AddonManager.PERM_API_CAN_UNINSTALL));
      }

      // Verify the add-ons file is in the right place
      let file = expectedDir.clone();
      file.append(id + ".xpi");
      Assert.ok(file.exists());
      Assert.ok(file.isFile());

      Assert.equal(getAddonFile(addon).path, file.path);

      if (isUpgrade) {
        Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_SYSTEM);
      }
    } else if (isUpgrade) {
      // Add-on should not be installed
      Assert.equal(addon, null);
    } else {
      // Either add-on should not be installed or it shouldn't be active
      Assert.ok(!addon || !addon.isActive);
    }
  }
}

// Test with a missing features directory
add_task(async function test_missing_app_dir() {
  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  await promiseStartupManager();

  let conditions = [
      { isUpgrade: false, version: null },
      { isUpgrade: false, version: null },
      { isUpgrade: false, version: null },
  ];

  await check_installed(conditions);

  Assert.ok(!updatesDir.exists());

  await promiseShutdownManager();
});

// Add some features in a new version
add_task(async function test_new_version() {
  gAppInfo.version = "1";
  distroDir.leafName = "app1";
  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  await promiseStartupManager();

  let conditions = [
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: false, version: null },
  ];

  await check_installed(conditions);

  Assert.ok(!updatesDir.exists());

  await promiseShutdownManager();
});

// Another new version swaps one feature and upgrades another
add_task(async function test_upgrade() {
  gAppInfo.version = "2";
  distroDir.leafName = "app2";
  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  await promiseStartupManager();

  let conditions = [
      { isUpgrade: false, version: "2.0" },
      { isUpgrade: false, version: null },
      { isUpgrade: false, version: "1.0" },
  ];

  await check_installed(conditions);

  Assert.ok(!updatesDir.exists());

  await promiseShutdownManager();
});

// Downgrade
add_task(async function test_downgrade() {
  gAppInfo.version = "1";
  distroDir.leafName = "app1";
  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  await promiseStartupManager();

  let conditions = [
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: false, version: null },
  ];

  await check_installed(conditions);

  Assert.ok(!updatesDir.exists());

  await promiseShutdownManager();
});

// Fake a mid-cycle install
add_task(async function test_updated() {
  // Create a random dir to install into
  let dirname = makeUUID();
  FileUtils.getDir("ProfD", ["features", dirname], true);
  updatesDir.append(dirname);

  // Copy in the system add-ons
  let file = await getSystemAddonXPI(2, "2.0");
  file.copyTo(updatesDir, "system2@tests.mozilla.org.xpi");
  file = await getSystemAddonXPI(3, "2.0");
  file.copyTo(updatesDir, "system3@tests.mozilla.org.xpi");

  // Inject it into the system set
  let addonSet = {
    schema: 1,
    directory: updatesDir.leafName,
    addons: {
      "system2@tests.mozilla.org": {
        version: "2.0",
      },
      "system3@tests.mozilla.org": {
        version: "2.0",
      },
    },
  };
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, JSON.stringify(addonSet));

  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  await promiseStartupManager();

  let conditions = [
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: true, version: "2.0" },
      { isUpgrade: true, version: "2.0" },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
});

// Entering safe mode should disable the updated system add-ons and use the
// default system add-ons
add_task(async function safe_mode_disabled() {
  gAppInfo.inSafeMode = true;
  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  await promiseStartupManager();

  let conditions = [
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: false, version: null },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
});

// Leaving safe mode should re-enable the updated system add-ons
add_task(async function normal_mode_enabled() {
  gAppInfo.inSafeMode = false;
  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  await promiseStartupManager();

  let conditions = [
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: true, version: "2.0" },
      { isUpgrade: true, version: "2.0" },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
});

// An additional add-on in the directory should be ignored
add_task(async function test_skips_additional() {
  // Copy in the system add-ons
  let file = await getSystemAddonXPI(4, "1.0");
  file.copyTo(updatesDir, "system4@tests.mozilla.org.xpi");

  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  await promiseStartupManager();

  let conditions = [
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: true, version: "2.0" },
      { isUpgrade: true, version: "2.0" },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
});

// Missing add-on should revert to the default set
add_task(async function test_revert() {
  manuallyUninstall(updatesDir, "system2@tests.mozilla.org");

  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  await promiseStartupManager();

  // With system add-on 2 gone the updated set is now invalid so it reverts to
  // the default set which is system add-ons 1 and 2.
  let conditions = [
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: false, version: null },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
});

// Putting it back will make the set work again
add_task(async function test_reuse() {
  let file = await getSystemAddonXPI(2, "2.0");
  file.copyTo(updatesDir, "system2@tests.mozilla.org.xpi");

  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  await promiseStartupManager();

  let conditions = [
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: true, version: "2.0" },
      { isUpgrade: true, version: "2.0" },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
});

// Making the pref corrupt should revert to the default set
add_task(async function test_corrupt_pref() {
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, "foo");

  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  await promiseStartupManager();

  let conditions = [
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: false, version: null },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
});

// An add-on with a bad certificate should cause us to use the default set
add_task(async function test_bad_profile_cert() {
  let file = await getSystemAddonXPI(1, "1.0");
  file.copyTo(updatesDir, "system1@tests.mozilla.org.xpi");

  // Inject it into the system set
  let addonSet = {
    schema: 1,
    directory: updatesDir.leafName,
    addons: {
      "system1@tests.mozilla.org": {
        version: "2.0",
      },
      "system2@tests.mozilla.org": {
        version: "1.0",
      },
      "system3@tests.mozilla.org": {
        version: "1.0",
      },
    },
  };
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, JSON.stringify(addonSet));

  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  await promiseStartupManager();

  let conditions = [
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: false, version: null },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
});

// Switching to app defaults that contain a bad certificate should still work
add_task(async function test_bad_app_cert() {
  gAppInfo.version = "3";
  distroDir.leafName = "app3";

  AddonTestUtils.usePrivilegedSignatures = id => {
    return (id === "system1@tests.mozilla.org") ? false : "system";
  };

  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  await promiseStartupManager();

  // Since we updated the app version, the system addon set should be reset as well.
  let addonSet = Services.prefs.getCharPref(PREF_SYSTEM_ADDON_SET);
  Assert.equal(addonSet, `{"schema":1,"addons":{}}`);

  // Add-on will still be present
  let addon = await promiseAddonByID("system1@tests.mozilla.org");
  Assert.notEqual(addon, null);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  let conditions = [
      { isUpgrade: false, version: "1.0" },
      { isUpgrade: false, version: null },
      { isUpgrade: false, version: "1.0" },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();

  AddonTestUtils.usePrivilegedSignatures = id => "system";
});

// A failed upgrade should revert to the default set.
add_task(async function test_updated_bad_update_set() {
  // Create a random dir to install into
  let dirname = makeUUID();
  FileUtils.getDir("ProfD", ["features", dirname], true);
  updatesDir.append(dirname);

  // Copy in the system add-ons
  let file = await getSystemAddonXPI(2, "2.0");
  file.copyTo(updatesDir, "system2@tests.mozilla.org.xpi");
  file = await getSystemAddonXPI("failed_update", "1.0");
  file.copyTo(updatesDir, "system_failed_update@tests.mozilla.org.xpi");

  // Inject it into the system set
  let addonSet = {
    schema: 1,
    directory: updatesDir.leafName,
    addons: {
      "system2@tests.mozilla.org": {
        version: "2.0",
      },
      "system_failed_update@tests.mozilla.org": {
        version: "1.0",
      },
    },
  };
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, JSON.stringify(addonSet));

  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  await promiseStartupManager();

  let conditions = [
      { isUpgrade: false, version: "1.0" },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
});
