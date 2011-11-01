/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests AddonManager.strictCompatibility and it's related preference,
// extensions.strictCompatibility


// Always compatible
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

// Incompatible in strict compatibility mode
var addon2 = {
  id: "addon2@tests.mozilla.org",
  version: "1.0",
  name: "Test 2",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "0.1",
    maxVersion: "0.2"
  }]
};

// Theme - always uses strict compatibility, so is always incompatible
var addon3 = {
  id: "addon3@tests.mozilla.org",
  version: "1.0",
  name: "Test 3",
  internalName: "test-theme-3",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "0.1",
    maxVersion: "0.2"
  }]
};


const profileDir = gProfD.clone();
profileDir.append("extensions");


function do_check_compat_status(aStrict, aAddonCompat, aCallback) {
  do_check_eq(AddonManager.strictCompatibility, aStrict);
  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org"],
                              function([a1, a2, a3]) {
    do_check_neq(a1, null);
    do_check_eq(a1.isCompatible, aAddonCompat[0]);
    do_check_eq(a1.appDisabled, !aAddonCompat[0]);

    do_check_neq(a2, null);
    do_check_eq(a2.isCompatible, aAddonCompat[1]);
    do_check_eq(a2.appDisabled, !aAddonCompat[1]);

    do_check_neq(a3, null);
    do_check_eq(a3.isCompatible, aAddonCompat[2]);
    do_check_eq(a3.appDisabled, !aAddonCompat[2]);

    aCallback();
  });
}


function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  
  writeInstallRDFForExtension(addon1, profileDir);
  writeInstallRDFForExtension(addon2, profileDir);
  writeInstallRDFForExtension(addon3, profileDir);

  startupManager();
  
  // Should default to enabling strict compat.
  do_check_compat_status(true, [true, false, false], run_test_1);
}

function run_test_1() {
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);
  do_check_compat_status(false, [true, true, false], run_test_2);
}

function run_test_2() {
  restartManager();
  do_check_compat_status(false, [true, true, false], run_test_3);
}

function run_test_3() {
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, true);
  do_check_compat_status(true, [true, false, false], run_test_4);
}

function run_test_4() {
  restartManager();
  do_check_compat_status(true, [true, false, false], do_test_finished);
}
