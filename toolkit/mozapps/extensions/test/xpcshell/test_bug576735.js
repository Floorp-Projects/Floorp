/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that we recover gracefully from an extension directory disappearing
// when we were expecting to uninstall it.

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

var addon2 = {
  id: "addon2@tests.mozilla.org",
  version: "2.0",
  name: "Test 2",
  targetApplications: [{
    id: "toolkit@mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "2");

  var dest = profileDir.clone();
  dest.append("addon1@tests.mozilla.org");
  writeInstallRDFToDir(addon1, dest);

  startupManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    a1.uninstall();

    shutdownManager();

    var dest = profileDir.clone();
    dest.append("addon1@tests.mozilla.org");
    dest.remove(true);

    dest = profileDir.clone();
    dest.append("addon2@tests.mozilla.org");
    writeInstallRDFToDir(addon2, dest);

    startupManager();

    AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                 "addon2@tests.mozilla.org"],
                                function([a1, a2]) {
      // Addon1 should no longer be installed
      do_check_eq(a1, null);

      // Addon2 should have been detected
      do_check_neq(a2, null);

      do_test_finished();
    });
  });
}
