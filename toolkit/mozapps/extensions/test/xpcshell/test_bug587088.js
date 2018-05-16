/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This test is currently dead code.
/* eslint-disable */

// Tests that trying to upgrade or uninstall an extension that has a file locked
// will roll back the upgrade or uninstall and retry at the next restart

const profileDir = gProfD.clone();
profileDir.append("extensions");

const ADDONS = [
  {
    "install.rdf": {
      "id": "addon1@tests.mozilla.org",
      "version": "1.0",
      "name": "Bug 587088 Test",
      "targetApplications": [
        {
          "id": "xpcshell@tests.mozilla.org",
          "minVersion": "1",
          "maxVersion": "1"
        }
      ]
    },
    "testfile": "",
    "testfile1": "",
  },

  {
    "install.rdf": {
      "id": "addon1@tests.mozilla.org",
      "version": "2.0",
      "name": "Bug 587088 Test",
      "targetApplications": [
        {
          "id": "xpcshell@tests.mozilla.org",
          "minVersion": "1",
          "maxVersion": "1"
        }
      ]
    },
    "testfile": "",
    "testfile2": "",
  },
];

add_task(async function setup() {
  // This is only an issue on windows.
  if (!("nsIWindowsRegKey" in Ci))
    return;

  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
});

function check_addon(aAddon, aVersion) {
  Assert.notEqual(aAddon, null);
  Assert.equal(aAddon.version, aVersion);
  Assert.ok(aAddon.isActive);
  Assert.ok(isExtensionInAddonsList(profileDir, aAddon.id));

  Assert.ok(aAddon.hasResource("testfile"));
  if (aVersion == "1.0") {
    Assert.ok(aAddon.hasResource("testfile1"));
    Assert.ok(!aAddon.hasResource("testfile2"));
  } else {
    Assert.ok(!aAddon.hasResource("testfile1"));
    Assert.ok(aAddon.hasResource("testfile2"));
  }

  Assert.equal(aAddon.pendingOperations, AddonManager.PENDING_NONE);
}

function check_addon_upgrading(aAddon) {
  Assert.notEqual(aAddon, null);
  Assert.equal(aAddon.version, "1.0");
  Assert.ok(aAddon.isActive);
  Assert.ok(isExtensionInAddonsList(profileDir, aAddon.id));

  Assert.ok(aAddon.hasResource("testfile"));
  Assert.ok(aAddon.hasResource("testfile1"));
  Assert.ok(!aAddon.hasResource("testfile2"));

  Assert.equal(aAddon.pendingOperations, AddonManager.PENDING_UPGRADE);

  Assert.equal(aAddon.pendingUpgrade.version, "2.0");
}

function check_addon_uninstalling(aAddon, aAfterRestart) {
  Assert.notEqual(aAddon, null);
  Assert.equal(aAddon.version, "1.0");

  if (aAfterRestart) {
    Assert.ok(!aAddon.isActive);
    Assert.ok(!isExtensionInAddonsList(profileDir, aAddon.id));
  } else {
    Assert.ok(aAddon.isActive);
    Assert.ok(isExtensionInAddonsList(profileDir, aAddon.id));
  }

  Assert.ok(aAddon.hasResource("testfile"));
  Assert.ok(aAddon.hasResource("testfile1"));
  Assert.ok(!aAddon.hasResource("testfile2"));

  Assert.equal(aAddon.pendingOperations, AddonManager.PENDING_UNINSTALL);
}

add_task(async function test_1() {
  await AddonTestUtils.promiseInstallXPI(ADDONS[0]);

  await promiseRestartManager();

  let a1 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  check_addon(a1, "1.0");

  // Lock either install.rdf for unpacked add-ons or the xpi for packed add-ons.
  let uri = a1.getResourceURI("install.rdf");
  if (uri.schemeIs("jar"))
    uri = a1.getResourceURI();

  let fstream = Cc["@mozilla.org/network/file-input-stream;1"].
                createInstance(Ci.nsIFileInputStream);
  fstream.init(uri.QueryInterface(Ci.nsIFileURL).file, -1, 0, 0);

  await AddonTestUtils.promiseInstallXPI(ADDONS[1]);

  check_addon_upgrading(a1);

  await promiseRestartManager();

  let a1_2 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  check_addon_upgrading(a1_2);

  await promiseRestartManager();

  let a1_3 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  check_addon_upgrading(a1_3);

  fstream.close();

  await promiseRestartManager();

  let a1_4 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  check_addon(a1_4, "2.0");

  a1_4.uninstall();
});

// Test that a failed uninstall gets rolled back
add_task(async function test_2() {
  await promiseRestartManager();

  await AddonTestUtils.promiseInstallXPI(ADDONS[0]);
  await promiseRestartManager();

  let a1 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  check_addon(a1, "1.0");

  // Lock either install.rdf for unpacked add-ons or the xpi for packed add-ons.
  let uri = a1.getResourceURI("install.rdf");
  if (uri.schemeIs("jar"))
    uri = a1.getResourceURI();

  let fstream = Cc["@mozilla.org/network/file-input-stream;1"].
                createInstance(Ci.nsIFileInputStream);
  fstream.init(uri.QueryInterface(Ci.nsIFileURL).file, -1, 0, 0);

  a1.uninstall();

  check_addon_uninstalling(a1);

  await promiseRestartManager();

  let a1_2 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  check_addon_uninstalling(a1_2, true);

  await promiseRestartManager();

  let a1_3 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  check_addon_uninstalling(a1_3, true);

  fstream.close();

  await promiseRestartManager();

  let a1_4 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.equal(a1_4, null);
  var dir = profileDir.clone();
  dir.append(do_get_expected_addon_name("addon1@tests.mozilla.org"));
  Assert.ok(!dir.exists());
  Assert.ok(!isExtensionInAddonsList(profileDir, "addon1@tests.mozilla.org"));
});
