/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that strange characters in an add-on version don't break the
// crash annotation.

var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "1,0",
  name: "Test 1",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var addon2 = {
  id: "addon2@tests.mozilla.org",
  version: "1:0",
  name: "Test 2",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var addon3 = {
  id: "addon3@tests.mozilla.org",
  version: "1,0",
  name: "Test 3",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var addon4 = {
  id: "addon4@tests.mozilla.org",
  version: "1:0",
  name: "Test 4",
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  do_test_pending();

  writeInstallRDFForExtension(addon1, profileDir);
  writeInstallRDFForExtension(addon2, profileDir);
  writeInstallRDFForExtension(addon3, profileDir);
  writeInstallRDFForExtension(addon4, profileDir);

  startupManager();

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org"],
                               function([a1, a2, a3, a4]) {

    Assert.notEqual(a1, null);
    do_check_in_crash_annotation(addon1.id, addon1.version);
    Assert.notEqual(a2, null);
    do_check_in_crash_annotation(addon2.id, addon2.version);
    Assert.notEqual(a3, null);
    do_check_in_crash_annotation(addon3.id, addon3.version);
    Assert.notEqual(a4, null);
    do_check_in_crash_annotation(addon4.id, addon4.version);

    do_execute_soon(do_test_finished);
  });
}
