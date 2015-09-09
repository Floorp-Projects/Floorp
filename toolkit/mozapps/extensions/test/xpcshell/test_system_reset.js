// Tests that we reset to the default system add-ons correctly when switching
// application versions
const PREF_SYSTEM_ADDON_SET = "extensions.systemAddonSet";

// Enable signature checks for these tests
Services.prefs.setBoolPref(PREF_XPI_SIGNATURES_REQUIRED, true);

const featureDir = gProfD.clone();
featureDir.append("features");

const distroDir = do_get_file("data/system_addons/app0");
registerDirectory("XREAppDist", distroDir);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "0");

function makeUUID() {
  let uuidGen = AM_Cc["@mozilla.org/uuid-generator;1"].
                getService(AM_Ci.nsIUUIDGenerator);
  return uuidGen.generateUUID().toString();
}

function* check_installed(inProfile, ...versions) {
  let expectedDir;
  if (inProfile) {
    expectedDir = featureDir;
  }
  else {
    expectedDir = distroDir.clone();
    expectedDir.append("features");
  }

  for (let i = 0; i < versions.length; i++) {
    let id = "system" + (i + 1) + "@tests.mozilla.org";
    let addon = yield promiseAddonByID(id);

    if (versions[i]) {
      // Add-on should be installed
      do_check_neq(addon, null);
      do_check_eq(addon.version, versions[i]);
      do_check_true(addon.isActive);
      do_check_false(addon.foreignInstall);

      // Verify the add-ons file is in the right place
      let file = expectedDir.clone();
      file.append(id + ".xpi");
      do_check_true(file.exists());
      do_check_true(file.isFile());

      let uri = addon.getResourceURI(null);
      do_check_true(uri instanceof AM_Ci.nsIFileURL);
      do_check_eq(uri.file.path, file.path);

      do_check_eq(addon.signedState, AddonManager.SIGNEDSTATE_SYSTEM);

      // Verify the add-on actually started
      let installed = Services.prefs.getCharPref("bootstraptest." + id + ".active_version");
      do_check_eq(installed, versions[i]);
    }
    else {
      if (inProfile) {
        // Add-on should not be installed
        do_check_eq(addon, null);
      }
      else {
        // Either add-on should not be installed or it shouldn't be active
        do_check_true(!addon || !addon.isActive);
      }

      try {
        Services.prefs.getCharPref("bootstraptest." + id + ".active_version");
        do_throw("Expected pref to be missing");
      }
      catch (e) {
      }
    }
  }
}

// Test with a missing features directory
add_task(function* test_missing_app_dir() {
  startupManager();

  yield check_installed(false, null, null, null);

  do_check_false(featureDir.exists());

  yield promiseShutdownManager();
});

// Add some features in a new version
add_task(function* test_new_version() {
  gAppInfo.version = "1";
  distroDir.leafName = "app1";
  startupManager();

  yield check_installed(false, "1.0", "1.0", null);

  do_check_false(featureDir.exists());

  yield promiseShutdownManager();
});

// Another new version swaps one feature and upgrades another
add_task(function* test_upgrade() {
  gAppInfo.version = "2";
  distroDir.leafName = "app2";
  startupManager();

  yield check_installed(false, "2.0", null, "1.0");

  do_check_false(featureDir.exists());

  yield promiseShutdownManager();
});

// Downgrade
add_task(function* test_downgrade() {
  gAppInfo.version = "1";
  distroDir.leafName = "app1";
  startupManager();

  yield check_installed(false, "1.0", "1.0", null);

  do_check_false(featureDir.exists());

  yield promiseShutdownManager();
});

// Fake a mid-cycle install
add_task(function* test_updated() {
  // Create a random dir to install into
  let dirname = makeUUID();
  FileUtils.getDir("ProfD", ["features", dirname], true);
  featureDir.append(dirname);

  // Copy in the system add-ons
  let file = do_get_file("data/system_addons/app1/features/system2@tests.mozilla.org.xpi");
  file.copyTo(featureDir, file.leafName);
  file = do_get_file("data/system_addons/app2/features/system3@tests.mozilla.org.xpi");
  file.copyTo(featureDir, file.leafName);

  // Inject it into the system set
  let addonSet = {
    schema: 1,
    directory: featureDir.leafName,
    addons: {
      "system2@tests.mozilla.org": {
        version: "1.0"
      },
      "system3@tests.mozilla.org": {
        version: "1.0"
      },
    }
  };
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, JSON.stringify(addonSet));

  startupManager(false);

  yield check_installed(true, null, "1.0", "1.0");

  yield promiseShutdownManager();
});

// An additional add-on in the directory should be ignored
add_task(function* test_skips_additional() {
  // Copy in the system add-ons
  let file = do_get_file("data/system_addons/app1/features/system1@tests.mozilla.org.xpi");
  file.copyTo(featureDir, file.leafName);

  startupManager(false);

  yield check_installed(true, null, "1.0", "1.0");

  yield promiseShutdownManager();
});

// Missing add-on should revert to the default set
add_task(function* test_revert() {
  manuallyUninstall(featureDir, "system2@tests.mozilla.org");

  startupManager(false);

  // With system add-on 2 gone the updated set is now invalid so it reverts to
  // the default set which is system add-ons 1 and 2.
  yield check_installed(false, "1.0", "1.0", null);

  yield promiseShutdownManager();
});

// Putting it back will make the set work again
add_task(function* test_reuse() {
  let file = do_get_file("data/system_addons/app1/features/system2@tests.mozilla.org.xpi");
  file.copyTo(featureDir, file.leafName);

  startupManager(false);

  yield check_installed(true, null, "1.0", "1.0");

  yield promiseShutdownManager();
});

// Making the pref corrupt should revert to the default set
add_task(function* test_corrupt_pref() {
  Services.prefs.setCharPref(PREF_SYSTEM_ADDON_SET, "foo");

  startupManager(false);

  yield check_installed(false, "1.0", "1.0", null);

  yield promiseShutdownManager();
});

// An add-on with a bad certificate should cause us to use the default set
add_task(function* test_bad_profile_cert() {
  let file = do_get_file("data/system_addons/app3/features/system1@tests.mozilla.org.xpi");
  file.copyTo(featureDir, file.leafName);

  // Inject it into the system set
  let addonSet = {
    schema: 1,
    directory: featureDir.leafName,
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

  startupManager(false);

  yield check_installed(false, "1.0", "1.0", null);

  yield promiseShutdownManager();
});

// Switching to app defaults that contain a bad certificate should ignore the
// bad add-on
add_task(function* test_bad_app_cert() {
  gAppInfo.version = "3";
  distroDir.leafName = "app3";
  startupManager();

  // Add-on will still be present just not active
  let addon = yield promiseAddonByID("system1@tests.mozilla.org");
  do_check_neq(addon, null);
  do_check_eq(addon.signedState, AddonManager.SIGNEDSTATE_SIGNED);

  yield check_installed(false, null, null, "1.0");

  yield promiseShutdownManager();
});
