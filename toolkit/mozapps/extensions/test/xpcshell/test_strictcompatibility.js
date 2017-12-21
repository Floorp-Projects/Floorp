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

// Dictionary - incompatible in strict compatibility mode
var addon7 = {
  id: "addon7@tests.mozilla.org",
  version: "1.0",
  name: "Test 7",
  type: "64",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "0.8",
    maxVersion: "0.9"
  }]
};



const profileDir = gProfD.clone();
profileDir.append("extensions");


function do_check_compat_status(aStrict, aAddonCompat, aCallback) {
  Assert.equal(AddonManager.strictCompatibility, aStrict);
  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org",
                               "addon6@tests.mozilla.org",
                               "addon7@tests.mozilla.org"],
                              function([a1, a2, a3, a4, a5, a6, a7]) {
    Assert.notEqual(a1, null);
    Assert.equal(a1.isCompatible, aAddonCompat[0]);
    Assert.equal(a1.appDisabled, !aAddonCompat[0]);
    Assert.ok(!a1.strictCompatibility);

    Assert.notEqual(a2, null);
    Assert.equal(a2.isCompatible, aAddonCompat[1]);
    Assert.equal(a2.appDisabled, !aAddonCompat[1]);
    Assert.ok(!a2.strictCompatibility);

    Assert.notEqual(a3, null);
    Assert.equal(a3.isCompatible, aAddonCompat[2]);
    Assert.equal(a3.appDisabled, !aAddonCompat[2]);
    Assert.ok(a3.strictCompatibility);

    Assert.notEqual(a4, null);
    Assert.equal(a4.isCompatible, aAddonCompat[3]);
    Assert.equal(a4.appDisabled, !aAddonCompat[3]);
    Assert.ok(a4.strictCompatibility);

    Assert.notEqual(a5, null);
    Assert.equal(a5.isCompatible, aAddonCompat[4]);
    Assert.equal(a5.appDisabled, !aAddonCompat[4]);
    Assert.ok(!a5.strictCompatibility);

    Assert.notEqual(a6, null);
    Assert.equal(a6.isCompatible, aAddonCompat[5]);
    Assert.equal(a6.appDisabled, !aAddonCompat[5]);
    Assert.ok(!a6.strictCompatibility);

    Assert.notEqual(a7, null);
    Assert.equal(a7.isCompatible, aAddonCompat[6]);
    Assert.equal(a7.appDisabled, !aAddonCompat[6]);
    Assert.ok(!a7.strictCompatibility);

    do_execute_soon(aCallback);
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
  writeInstallRDFForExtension(addon7, profileDir);

  Services.prefs.setCharPref(PREF_EM_MIN_COMPAT_APP_VERSION, "0.1");

  startupManager();

  // Should default to enabling strict compat.
  do_check_compat_status(true, [true, false, false, false, false, false, false], run_test_1);
}

function run_test_1() {
  do_print("Test 1");
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);
  do_check_compat_status(false, [true, true, false, false, false, true, true], run_test_2);
}

function run_test_2() {
  do_print("Test 2");
  restartManager();
  do_check_compat_status(false, [true, true, false, false, false, true, true], run_test_3);
}

function run_test_3() {
  do_print("Test 3");
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, true);
  do_check_compat_status(true, [true, false, false, false, false, false, false], run_test_4);
}

function run_test_4() {
  do_print("Test 4");
  restartManager();
  do_check_compat_status(true, [true, false, false, false, false, false, false], run_test_5);
}

function run_test_5() {
  do_print("Test 5");
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, false);
  Services.prefs.setCharPref(PREF_EM_MIN_COMPAT_APP_VERSION, "0.4");
  do_check_compat_status(false, [true, true, false, false, false, false, true], do_test_finished);
}
