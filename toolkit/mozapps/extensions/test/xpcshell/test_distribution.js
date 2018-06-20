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

const ADDONS = {
  test_distribution1_2: {
    "install.rdf": {
      "id": "addon1@tests.mozilla.org",
      "version": "2.0",
      "name": "Distributed add-ons test",
    }
  },
};

var addon1_1 = {
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  name: "Test version 1",
  bootstrap: true,
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
  bootstrap: true,
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
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "5"
  }]
};

async function setOldModificationTime() {
  // Make sure the installed extension has an old modification time so any
  // changes will be detected
  await promiseShutdownManager();
  let extension = gProfD.clone();
  extension.append("extensions");
  extension.append("addon1@tests.mozilla.org.xpi");
  setExtensionModifiedTime(extension, Date.now() - MAKE_FILE_OLD_DIFFERENCE);
  await promiseStartupManager();
}

function run_test() {
  do_test_pending();

  run_test_1();
}

// Tests that on the first startup the add-on gets installed, with now as the
// profile modifiedTime.
async function run_test_1() {
  let extension = await promiseWriteInstallRDFForExtension(addon1_1, distroDir);
  setExtensionModifiedTime(extension, Date.now() - MAKE_FILE_OLD_DIFFERENCE);

  await promiseStartupManager();

  let a1 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(a1, null);
  Assert.equal(a1.version, "1.0");
  Assert.ok(a1.isActive);
  Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);
  Assert.ok(!a1.foreignInstall);

  // Modification time should be updated when the addon is copied to the
  // profile.
  let testURI = a1.getResourceURI(TEST_UNPACKED ? "install.rdf" : "");
  let testFile = testURI.QueryInterface(Ci.nsIFileURL).file;

  Assert.ok(testFile.exists());
  let difference = testFile.lastModifiedTime - Date.now();
  Assert.ok(Math.abs(difference) < MAX_TIME_DIFFERENCE);

  executeSoon(run_test_2);
}

// Tests that starting with a newer version in the distribution dir doesn't
// install it yet
async function run_test_2() {
  await setOldModificationTime();

  await promiseWriteInstallRDFForExtension(addon1_2, distroDir);

  await promiseRestartManager();

  let a1 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(a1, null);
  Assert.equal(a1.version, "1.0");
  Assert.ok(a1.isActive);
  Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);

  executeSoon(run_test_3);
}

// Test that an app upgrade installs the newer version
async function run_test_3() {
  await promiseRestartManager("2");

  let a1 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(a1, null);
  Assert.equal(a1.version, "2.0");
  Assert.ok(a1.isActive);
  Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);
  Assert.ok(!a1.foreignInstall);

  executeSoon(run_test_4);
}

// Test that an app upgrade doesn't downgrade the extension
async function run_test_4() {
  await setOldModificationTime();

  await promiseWriteInstallRDFForExtension(addon1_1, distroDir);

  await promiseRestartManager("3");

  let a1 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(a1, null);
  Assert.equal(a1.version, "2.0");
  Assert.ok(a1.isActive);
  Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);

  executeSoon(run_test_5);
}

// Tests that after uninstalling a restart doesn't re-install the extension
async function run_test_5() {
  let a1 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  await a1.uninstall();

  await promiseRestartManager();

  let a1_2 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.equal(a1_2, null);

  executeSoon(run_test_6);
}

// Tests that upgrading the application still doesn't re-install the uninstalled
// extension
async function run_test_6() {
  await promiseRestartManager("4");

  let a1 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.equal(a1, null);

  executeSoon(run_test_7);
}

// Tests that a pending install of a newer version of a distributed add-on
// at app change still gets applied
async function run_test_7() {
  Services.prefs.clearUserPref("extensions.installedDistroAddon.addon1@tests.mozilla.org");

  await AddonTestUtils.promiseInstallXPI(ADDONS.test_distribution1_2);
  await promiseRestartManager(2);

  let a1 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(a1, null);
  Assert.equal(a1.version, "2.0");
  Assert.ok(a1.isActive);
  Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);

  await a1.uninstall();
  executeSoon(run_test_8);
}

// Tests that a pending install of a older version of a distributed add-on
// at app change gets replaced by the distributed version
async function run_test_8() {
  await promiseRestartManager();

  await promiseWriteInstallRDFForExtension(addon1_3, distroDir);

  await AddonTestUtils.promiseInstallXPI(ADDONS.test_distribution1_2);
  await promiseRestartManager(3);

  let a1 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(a1, null);
  Assert.equal(a1.version, "3.0");
  Assert.ok(a1.isActive);
  Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);

  await a1.uninstall();
  executeSoon(do_test_finished);
}
