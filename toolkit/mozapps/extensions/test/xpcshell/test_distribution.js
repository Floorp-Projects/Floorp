/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that add-ons distributed with the application get installed
// correctly

// Allow distributed add-ons to install
Services.prefs.setBoolPref("extensions.installDistroAddons", true);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

const profileDir = gProfD.clone();
profileDir.append("extensions");
const distroDir = gProfD.clone();
distroDir.append("distribution");
distroDir.append("extensions");
registerDirectory("XREAppDist", distroDir.parent);

var addon1_1 = {
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  name: "Test version 1",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "5"
  }]
};

var addon1_2 = {
  id: "addon1@tests.mozilla.org",
  version: "2.0",
  name: "Test version 2",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "5"
  }]
};

var addon1_3 = {
  id: "addon1@tests.mozilla.org",
  version: "3.0",
  name: "Test version 3",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "5"
  }]
};

function getActiveVersion() {
  return Services.prefs.getIntPref("bootstraptest.active_version");
}

function getInstalledVersion() {
  return Services.prefs.getIntPref("bootstraptest.installed_version");
}

function setOldModificationTime() {
  // Make sure the installed extension has an old modification time so any
  // changes will be detected
  shutdownManager();
  let extension = gProfD.clone();
  extension.append("extensions");
  if (Services.prefs.getBoolPref("extensions.alwaysUnpack"))
    extension.append("addon1@tests.mozilla.org");
  else
    extension.append("addon1@tests.mozilla.org.xpi");
  setExtensionModifiedTime(extension, Date.now() - MAKE_FILE_OLD_DIFFERENCE);
  startupManager(false);
}

function run_test() {
  do_test_pending();

  run_test_1();
}

// Tests that on the first startup the add-on gets installed, with now as the
// profile modifiedTime.
function run_test_1() {
  let extension = writeInstallRDFForExtension(addon1_1, distroDir);
  setExtensionModifiedTime(extension, Date.now() - MAKE_FILE_OLD_DIFFERENCE);

  startupManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    Assert.notEqual(a1, null);
    Assert.equal(a1.version, "1.0");
    Assert.ok(a1.isActive);
    Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);
    Assert.ok(!a1.foreignInstall);

    // Modification time should be updated when the addon is copied to the
    // profile.
    let testURI = a1.getResourceURI(TEST_UNPACKED ? "install.rdf" : "");
    let testFile = testURI.QueryInterface(Components.interfaces.nsIFileURL).file;

    Assert.ok(testFile.exists());
    let difference = testFile.lastModifiedTime - Date.now();
    Assert.ok(Math.abs(difference) < MAX_TIME_DIFFERENCE);

    executeSoon(run_test_2);
  });
}

// Tests that starting with a newer version in the distribution dir doesn't
// install it yet
function run_test_2() {
  setOldModificationTime();

  writeInstallRDFForExtension(addon1_2, distroDir);

  restartManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    Assert.notEqual(a1, null);
    Assert.equal(a1.version, "1.0");
    Assert.ok(a1.isActive);
    Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);

    executeSoon(run_test_3);
  });
}

// Test that an app upgrade installs the newer version
function run_test_3() {
  restartManager("2");

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    Assert.notEqual(a1, null);
    Assert.equal(a1.version, "2.0");
    Assert.ok(a1.isActive);
    Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);
    Assert.ok(!a1.foreignInstall);

    executeSoon(run_test_4);
  });
}

// Test that an app upgrade doesn't downgrade the extension
function run_test_4() {
  setOldModificationTime();

  writeInstallRDFForExtension(addon1_1, distroDir);

  restartManager("3");

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    Assert.notEqual(a1, null);
    Assert.equal(a1.version, "2.0");
    Assert.ok(a1.isActive);
    Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);

    executeSoon(run_test_5);
  });
}

// Tests that after uninstalling a restart doesn't re-install the extension
function run_test_5() {
  AddonManager.getAddonByID("addon1@tests.mozilla.org", callback_soon(function(a1) {
    a1.uninstall();

    restartManager();

    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1_2) {
      Assert.equal(a1_2, null);

      executeSoon(run_test_6);
    });
  }));
}

// Tests that upgrading the application still doesn't re-install the uninstalled
// extension
function run_test_6() {
  restartManager("4");

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    Assert.equal(a1, null);

    executeSoon(run_test_7);
  });
}

// Tests that a pending install of a newer version of a distributed add-on
// at app change still gets applied
function run_test_7() {
  Services.prefs.clearUserPref("extensions.installedDistroAddon.addon1@tests.mozilla.org");

  installAllFiles([do_get_addon("test_distribution1_2")], function() {
    restartManager(2);

    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
      Assert.notEqual(a1, null);
      Assert.equal(a1.version, "2.0");
      Assert.ok(a1.isActive);
      Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);

      a1.uninstall();
      executeSoon(run_test_8);
    });
  });
}

// Tests that a pending install of a older version of a distributed add-on
// at app change gets replaced by the distributed version
function run_test_8() {
  restartManager();

  writeInstallRDFForExtension(addon1_3, distroDir);

  installAllFiles([do_get_addon("test_distribution1_2")], function() {
    restartManager(3);

    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
      Assert.notEqual(a1, null);
      Assert.equal(a1.version, "3.0");
      Assert.ok(a1.isActive);
      Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);

      a1.uninstall();
      executeSoon(run_test_9);
    });
  });
}

// Tests that bootstrapped add-ons distributed start up correctly, also that
// add-ons with multiple directories get copied fully
function run_test_9() {
  restartManager();

  // Copy the test add-on to the distro dir
  let addon = do_get_file("data/test_distribution2_2");
  addon.copyTo(distroDir, "addon2@tests.mozilla.org");

  restartManager("5");

  AddonManager.getAddonByID("addon2@tests.mozilla.org", function(a2) {
    Assert.notEqual(a2, null);
    Assert.ok(a2.isActive);

    Assert.equal(getInstalledVersion(), 2);
    Assert.equal(getActiveVersion(), 2);

    Assert.ok(a2.hasResource("bootstrap.js"));
    Assert.ok(a2.hasResource("subdir/dummy.txt"));
    Assert.ok(a2.hasResource("subdir/subdir2/dummy2.txt"));

    // Currently installs are unpacked if the source is a directory regardless
    // of the install.rdf property or the global preference

    let addonDir = profileDir.clone();
    addonDir.append("addon2@tests.mozilla.org");
    Assert.ok(addonDir.exists());
    Assert.ok(addonDir.isDirectory());
    addonDir.append("subdir");
    Assert.ok(addonDir.exists());
    Assert.ok(addonDir.isDirectory());
    addonDir.append("subdir2");
    Assert.ok(addonDir.exists());
    Assert.ok(addonDir.isDirectory());
    addonDir.append("dummy2.txt");
    Assert.ok(addonDir.exists());
    Assert.ok(addonDir.isFile());

    a2.uninstall();

    executeSoon(do_test_finished);
  });
}
