/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Checks that we migrate data from previous versions of the database

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
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var addon3 = {
  id: "addon3@tests.mozilla.org",
  version: "2.0",
  name: "Test 3",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var addon4 = {
  id: "addon4@tests.mozilla.org",
  version: "2.0",
  name: "Test 4",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  var dest = profileDir.clone();
  dest.append("addon1@tests.mozilla.org");
  writeInstallRDFToDir(addon1, dest);
  dest = profileDir.clone();
  dest.append("addon2@tests.mozilla.org");
  writeInstallRDFToDir(addon2, dest);
  dest = profileDir.clone();
  dest.append("addon3@tests.mozilla.org");
  writeInstallRDFToDir(addon3, dest);
  dest = profileDir.clone();
  dest.append("addon4@tests.mozilla.org");
  writeInstallRDFToDir(addon4, dest);

  let old = do_get_file("data/test_migrate.rdf");
  old.copyTo(gProfD, "extensions.rdf");

  startupManager(1);
  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org"], function([a1, a2, a3, a4]) {
    // addon1 was enabled in the old extensions.rdf
    do_check_neq(a1, null);
    do_check_false(a1.userDisabled);
    // addon1 was disabled in the old extensions.rdf
    do_check_neq(a2, null);
    do_check_true(a2.userDisabled);
    // addon1 was pending-disable in the old extensions.rdf
    do_check_neq(a3, null);
    do_check_true(a3.userDisabled);
    // addon1 was pending-enable in the old extensions.rdf
    do_check_neq(a4, null);
    do_check_false(a4.userDisabled);
    do_test_finished();
  });
}
