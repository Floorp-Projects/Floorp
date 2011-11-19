/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests AddonManager.strictCompatibility and it's related preference,
// extensions.strictCompatibility, and the strictCompatibility option in
// install.rdf


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
    minVersion: "0.7",
    maxVersion: "0.8"
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
    minVersion: "0.8",
    maxVersion: "0.9"
  }]
};

// Opt-in to strict compatibility - always incompatible
var addon4 = {
  id: "addon4@tests.mozilla.org",
  version: "1.0",
  name: "Test 4",
  strictCompatibility: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "0.8",
    maxVersion: "0.9"
  }]
};

// Addon from the future - would be marked as compatibile-by-default,
// but minVersion is higher than the app version
var addon5 = {
  id: "addon5@tests.mozilla.org",
  version: "1.0",
  name: "Test 5",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "3",
    maxVersion: "5"
  }]
};

// Extremely old addon - maxVersion is less than the mimimum compat version
// set in extensions.minCompatibleVersion
var addon6 = {
  id: "addon6@tests.mozilla.org",
  version: "1.0",
  name: "Test 6",
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
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org",
                               "addon6@tests.mozilla.org"],
                              function([a1, a2, a3, a4, a5, a6]) {
    do_check_neq(a1, null);
    do_check_eq(a1.isCompatible, aAddonCompat[0]);
    do_check_eq(a1.appDisabled, !aAddonCompat[0]);
    do_check_false(a1.strictCompatibility);

    do_check_neq(a2, null);
    do_check_eq(a2.isCompatible, aAddonCompat[1]);
    do_check_eq(a2.appDisabled, !aAddonCompat[1]);
    do_check_false(a2.strictCompatibility);

    do_check_neq(a3, null);
    do_check_eq(a3.isCompatible, aAddonCompat[2]);
    do_check_eq(a3.appDisabled, !aAddonCompat[2]);
    do_check_true(a3.strictCompatibility);

    do_check_neq(a4, null);
    do_check_eq(a4.isCompatible, aAddonCompat[3]);
    do_check_eq(a4.appDisabled, !aAddonCompat[3]);
    do_check_true(a4.strictCompatibility);

    do_check_neq(a5, null);
    do_check_eq(a5.isCompatible, aAddonCompat[4]);
    do_check_eq(a5.appDisabled, !aAddonCompat[4]);
    do_check_false(a5.strictCompatibility);

    do_check_neq(a6, null);
    do_check_eq(a6.isCompatible, aAddonCompat[5]);
    do_check_eq(a6.appDisabled, !aAddonCompat[5]);
    do_check_false(a6.strictCompatibility);

    aCallback();
  });
}


function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  
  writeInstallRDFForExtension(addon1, profileDir);
  writeInstallRDFForExtension(addon2, profileDir);
  writeInstallRDFForExtension(addon3, profileDir);
  writeInstallRDFForExtension(addon4, profileDir);
  writeInstallRDFForExtension(addon5, profileDir);
  writeInstallRDFForExtension(addon6, profileDir);

  Services.prefs.setCharPref(PREF_EM_MIN_COMPAT_APP_VERSION, "0.1");

  startupManager();

  // Should default to enabling strict compat.
  do_check_compat_status(true, [true, false, false, false, false, false], run_test_1);
}

function run_test_1() {
  do_print("Test 1");
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);
  do_check_compat_status(false, [true, true, false, false, false, true], run_test_2);
}

function run_test_2() {
  do_print("Test 2");
  restartManager();
  do_check_compat_status(false, [true, true, false, false, false, true], run_test_3);
}

function run_test_3() {
  do_print("Test 3");
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, true);
  do_check_compat_status(true, [true, false, false, false, false, false], run_test_4);
}

function run_test_4() {
  do_print("Test 4");
  restartManager();
  do_check_compat_status(true, [true, false, false, false, false, false], run_test_5);
}

function run_test_5() {
  do_print("Test 5");
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);
  Services.prefs.setCharPref(PREF_EM_MIN_COMPAT_APP_VERSION, "0.4");
  do_check_compat_status(false, [true, true, false, false, false, false], do_test_finished);
}