/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that add-ons with invalid target application entries show
// up in the API but are correctly appDisabled

// A working add-on
var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  name: "Test 1",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Missing id
var addon2 = {
  id: "addon2@tests.mozilla.org",
  version: "1.0",
  name: "Test 2",
  targetApplications: [{
    minVersion: "1",
    maxVersion: "2"
  }]
};

// Missing minVersion
var addon3 = {
  id: "addon3@tests.mozilla.org",
  version: "1.0",
  name: "Test 3",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    maxVersion: "1"
  }]
};

// Missing maxVersion
var addon4 = {
  id: "addon4@tests.mozilla.org",
  version: "1.0",
  name: "Test 4",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1"
  }]
};

// Blank id
var addon5 = {
  id: "addon5@tests.mozilla.org",
  version: "1.0",
  name: "Test 5",
  targetApplications: [{
    id: "",
    minVersion: "1",
    maxVersion: "2"
  }]
};

// Blank minVersion
var addon6 = {
  id: "addon6@tests.mozilla.org",
  version: "1.0",
  name: "Test 6",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "",
    maxVersion: "1"
  }]
};

// Blank maxVersion
var addon7 = {
  id: "addon7@tests.mozilla.org",
  version: "1.0",
  name: "Test 7",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: ""
  }]
};

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

const profileDir = gProfD.clone();
profileDir.append("extensions");

// Set up the profile
function run_test() {
  do_test_pending();

  writeInstallRDFForExtension(addon1, profileDir);
  writeInstallRDFForExtension(addon2, profileDir);
  writeInstallRDFForExtension(addon3, profileDir);
  writeInstallRDFForExtension(addon4, profileDir);
  writeInstallRDFForExtension(addon5, profileDir);
  writeInstallRDFForExtension(addon6, profileDir);
  writeInstallRDFForExtension(addon7, profileDir);

  startupManager();

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org",
                               "addon6@tests.mozilla.org",
                               "addon7@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5, a6, a7]) {
    Assert.notEqual(a1, null);
    Assert.ok(!a1.appDisabled);
    Assert.ok(a1.isActive);

    Assert.notEqual(a2, null);
    Assert.ok(a2.appDisabled);
    Assert.ok(!a2.isActive);

    Assert.notEqual(a3, null);
    Assert.ok(a3.appDisabled);
    Assert.ok(!a3.isActive);

    Assert.notEqual(a4, null);
    Assert.ok(a4.appDisabled);
    Assert.ok(!a4.isActive);

    Assert.notEqual(a5, null);
    Assert.ok(a5.appDisabled);
    Assert.ok(!a5.isActive);

    Assert.notEqual(a6, null);
    Assert.ok(a6.appDisabled);
    Assert.ok(!a6.isActive);

    Assert.notEqual(a6, null);
    Assert.ok(a6.appDisabled);
    Assert.ok(!a6.isActive);

    executeSoon(do_test_finished);

  });
}
