// Tests that we reset to the default system add-ons correctly when switching
// application versions

BootstrapMonitor.init();

const updatesDir = FileUtils.getDir("ProfD", ["features"]);

// Build the test sets
var dir = FileUtils.getDir("ProfD", ["sysfeatures", "app1"], true);
do_get_file("data/system_addons/system1_1.xpi").copyTo(dir, "system1@tests.mozilla.org.xpi");
do_get_file("data/system_addons/system2_1.xpi").copyTo(dir, "system2@tests.mozilla.org.xpi");

dir = FileUtils.getDir("ProfD", ["sysfeatures", "app2"], true);
do_get_file("data/system_addons/system1_2.xpi").copyTo(dir, "system1@tests.mozilla.org.xpi");
do_get_file("data/system_addons/system3_1.xpi").copyTo(dir, "system3@tests.mozilla.org.xpi");

dir = FileUtils.getDir("ProfD", ["sysfeatures", "app3"], true);
do_get_file("data/system_addons/system1_1_badcert.xpi").copyTo(dir, "system1@tests.mozilla.org.xpi");
do_get_file("data/system_addons/system3_1.xpi").copyTo(dir, "system3@tests.mozilla.org.xpi");

const distroDir = FileUtils.getDir("ProfD", ["sysfeatures", "app0"], true);
registerDirectory("XREAppFeat", distroDir);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "0");

function makeUUID() {
  let uuidGen = AM_Cc["@mozilla.org/uuid-generator;1"].
                getService(AM_Ci.nsIUUIDGenerator);
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
      do_check_neq(addon, null);
      do_check_eq(addon.version, version);
      do_check_true(addon.isActive);
      do_check_false(addon.foreignInstall);
      do_check_true(addon.hidden);
      do_check_true(addon.isSystem);
      do_check_false(hasFlag(addon.permissions, AddonManager.PERM_CAN_UPGRADE));
      if (isUpgrade) {
        do_check_true(hasFlag(addon.permissions, AddonManager.PERM_CAN_UNINSTALL));
      } else {
        do_check_false(hasFlag(addon.permissions, AddonManager.PERM_CAN_UNINSTALL));
      }

      // Verify the add-ons file is in the right place
      let file = expectedDir.clone();
      file.append(id + ".xpi");
      do_check_true(file.exists());
      do_check_true(file.isFile());

      let uri = addon.getResourceURI(null);
      do_check_true(uri instanceof AM_Ci.nsIFileURL);
      do_check_eq(uri.file.path, file.path);

      if (isUpgrade) {
        do_check_eq(addon.signedState, AddonManager.SIGNEDSTATE_SYSTEM);
      }

      // Verify the add-on actually started
      BootstrapMonitor.checkAddonStarted(id, version);
    } else {
      if (isUpgrade) {
        // Add-on should not be installed
        do_check_eq(addon, null);
      } else {
        // Either add-on should not be installed or it shouldn't be active
        do_check_true(!addon || !addon.isActive);
      }

      BootstrapMonitor.checkAddonNotStarted(id);

      if (addon)
        BootstrapMonitor.checkAddonInstalled(id);
      else
        BootstrapMonitor.checkAddonNotInstalled(id);
    }
  }
}

// Test with a missing features directory
add_task(async function test_missing_app_dir() {
  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  startupManager();

  let conditions = [
    { isUpgrade: false, version: null },
    { isUpgrade: false, version: null },
    { isUpgrade: false, version: null },
  ];

  await check_installed(conditions);

  do_check_false(updatesDir.exists());

  await promiseShutdownManager();
});

// Add some features in a new version
add_task(async function test_new_version() {
  gAppInfo.version = "1";
  distroDir.leafName = "app1";
  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  startupManager();

  let conditions = [
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: null },
  ];

  await check_installed(conditions);

  do_check_false(updatesDir.exists());

  await promiseShutdownManager();
});

// Another new version swaps one feature and upgrades another
add_task(async function test_upgrade() {
  gAppInfo.version = "2";
  distroDir.leafName = "app2";
  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  startupManager();

  let conditions = [
    { isUpgrade: false, version: "2.0" },
    { isUpgrade: false, version: null },
    { isUpgrade: false, version: "1.0" },
  ];

  await check_installed(conditions);

  do_check_false(updatesDir.exists());

  await promiseShutdownManager();
});

// Downgrade
add_task(async function test_downgrade() {
  gAppInfo.version = "1";
  distroDir.leafName = "app1";
  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  startupManager();

  let conditions = [
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: null },
  ];

  await check_installed(conditions);

  do_check_false(updatesDir.exists());

  await promiseShutdownManager();
});

// Fake a mid-cycle install
add_task(async function test_updated() {
  // Create a random dir to install into
  let dirname = makeUUID();
  FileUtils.getDir("ProfD", ["features", dirname], true);
  updatesDir.append(dirname);

  // Copy in the system add-ons
  let file = do_get_file("data/system_addons/system2_2.xpi");
  file.copyTo(updatesDir, "system2@tests.mozilla.org.xpi");
  file = do_get_file("data/system_addons/system3_2.xpi");
  file.copyTo(updatesDir, "system3@tests.mozilla.org.xpi");

  // Inject it into the system set
  let addonSet = {
    schema: 1,
    directory: updatesDir.leafName,
    addons: {
      "system2@tests.mozilla.org": {
        version: "2.0"
      },
      "system3@tests.mozilla.org": {
        version: "2.0"
      },
    }
  };
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, JSON.stringify(addonSet));

  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  startupManager(false);

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
  startupManager(false);

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
  startupManager(false);

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
  let file = do_get_file("data/system_addons/system4_1.xpi");
  file.copyTo(updatesDir, "system4@tests.mozilla.org.xpi");

  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  startupManager(false);

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

  // With the add-on physically gone from disk we won't see uninstall events
  BootstrapMonitor.clear("system2@tests.mozilla.org");

  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  startupManager(false);

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
  let file = do_get_file("data/system_addons/system2_2.xpi");
  file.copyTo(updatesDir, "system2@tests.mozilla.org.xpi");

  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  startupManager(false);

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
  startupManager(false);

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
  let file = do_get_file("data/system_addons/system1_1_badcert.xpi");
  file.copyTo(updatesDir, "system1@tests.mozilla.org.xpi");

  // Inject it into the system set
  let addonSet = {
    schema: 1,
    directory: updatesDir.leafName,
    addons: {
      "system1@tests.mozilla.org": {
        version: "2.0"
      },
      "system2@tests.mozilla.org": {
        version: "1.0"
      },
      "system3@tests.mozilla.org": {
        version: "1.0"
      },
    }
  };
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, JSON.stringify(addonSet));

  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  startupManager(false);

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
  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  startupManager();

  // Since we updated the app version, the system addon set should be reset as well.
  let addonSet = Services.prefs.getCharPref(PREF_SYSTEM_ADDON_SET);
  do_check_eq(addonSet, `{"schema":1,"addons":{}}`);

  // Add-on will still be present
  let addon = await promiseAddonByID("system1@tests.mozilla.org");
  do_check_neq(addon, null);
  do_check_eq(addon.signedState, AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  let conditions = [
    { isUpgrade: false, version: "1.0" },
    { isUpgrade: false, version: null },
    { isUpgrade: false, version: "1.0" },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
});

// A failed upgrade should revert to the default set.
add_task(async function test_updated_bad_update_set() {
  // Create a random dir to install into
  let dirname = makeUUID();
  FileUtils.getDir("ProfD", ["features", dirname], true);
  updatesDir.append(dirname);

  // Copy in the system add-ons
  let file = do_get_file("data/system_addons/system2_2.xpi");
  file.copyTo(updatesDir, "system2@tests.mozilla.org.xpi");
  file = do_get_file("data/system_addons/system_failed_update.xpi");
  file.copyTo(updatesDir, "system_failed_update@tests.mozilla.org.xpi");

  // Inject it into the system set
  let addonSet = {
    schema: 1,
    directory: updatesDir.leafName,
    addons: {
      "system2@tests.mozilla.org": {
        version: "2.0"
      },
      "system_failed_update@tests.mozilla.org": {
        version: "1.0"
      },
    }
  };
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, JSON.stringify(addonSet));

  await overrideBuiltIns({ "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org", "system3@tests.mozilla.org", "system5@tests.mozilla.org"] });
  startupManager(false);

  let conditions = [
    { isUpgrade: false, version: "1.0" },
  ];

  await check_installed(conditions);

  await promiseShutdownManager();
});
