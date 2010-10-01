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
    id: "toolkit@mozilla.org",
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
    id: "toolkit@mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var addon5 = {
  id: "addon5@tests.mozilla.org",
  version: "2.0",
  name: "Test 5",
  targetApplications: [{
    id: "toolkit@mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var theme1 = {
  id: "theme1@tests.mozilla.org",
  version: "1.0",
  name: "Theme 1",
  type: 4,
  internalName: "theme1/1.0",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "2"
  }]
};

var theme2 = {
  id: "theme2@tests.mozilla.org",
  version: "1.0",
  name: "Theme 2",
  type: 4,
  internalName: "theme2/1.0",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "2"
  }]
};

const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  writeInstallRDFForExtension(addon1, profileDir);
  writeInstallRDFForExtension(addon2, profileDir);
  writeInstallRDFForExtension(addon3, profileDir);
  writeInstallRDFForExtension(addon4, profileDir);
  writeInstallRDFForExtension(addon5, profileDir);
  writeInstallRDFForExtension(theme1, profileDir);
  writeInstallRDFForExtension(theme2, profileDir);

  let old = do_get_file("data/test_migrate.rdf");
  old.copyTo(gProfD, "extensions.rdf");

  // Theme state is determined by the selected theme pref
  Services.prefs.setCharPref("general.skins.selectedSkin", "theme1/1.0");

  startupManager();
  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org",
                               "theme1@tests.mozilla.org",
                               "theme2@tests.mozilla.org"], function([a1, a2,
                                                                      a3, a4, a5,
                                                                      t1, t2]) {
    // addon1 was user and app enabled in the old extensions.rdf
    do_check_neq(a1, null);
    do_check_false(a1.userDisabled);
    do_check_false(a1.appDisabled);

    // addon2 was user disabled and app enabled in the old extensions.rdf
    do_check_neq(a2, null);
    do_check_true(a2.userDisabled);
    do_check_false(a2.appDisabled);

    // addon3 was pending user disable and app disabled in the old extensions.rdf
    do_check_neq(a3, null);
    do_check_true(a3.userDisabled);
    do_check_true(a3.appDisabled);

    // addon4 was pending user enable and app disabled in the old extensions.rdf
    do_check_neq(a4, null);
    do_check_false(a4.userDisabled);
    do_check_true(a4.appDisabled);

    // addon5 was disabled and compatible but a new version has been installed
    // since, it should still be disabled but should be incompatible
    do_check_neq(a5, null);
    do_check_true(a5.userDisabled);
    do_check_true(a5.appDisabled);

    // Theme 1 was previously enabled
    do_check_neq(t1, null);
    do_check_false(t1.userDisabled);
    do_check_false(t1.appDisabled);
    do_check_true(t1.isActive);
    do_check_false(hasFlag(t1.permissions, AddonManager.PERM_CAN_ENABLE));

    // Theme 2 was previously disabled
    do_check_neq(t1, null);
    do_check_true(t2.userDisabled);
    do_check_false(t2.appDisabled);
    do_check_false(t2.isActive);
    do_check_true(hasFlag(t2.permissions, AddonManager.PERM_CAN_ENABLE));

    do_test_finished();
  });
}
