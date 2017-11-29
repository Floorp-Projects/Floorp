/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that the targetPlatform entries are checked when deciding
// if an add-on is incompatible.

// No targetPlatforms so should be compatible
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

// Matches the OS
var addon2 = {
  id: "addon2@tests.mozilla.org",
  version: "1.0",
  name: "Test 2",
  targetPlatforms: [
    "XPCShell",
    "WINNT_x86",
    "XPCShell"
  ],
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Matches the OS and ABI
var addon3 = {
  id: "addon3@tests.mozilla.org",
  version: "1.0",
  name: "Test 3",
  targetPlatforms: [
    "WINNT",
    "XPCShell_noarch-spidermonkey"
  ],
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Doesn't match
var addon4 = {
  id: "addon4@tests.mozilla.org",
  version: "1.0",
  name: "Test 4",
  targetPlatforms: [
    "WINNT_noarch-spidermonkey",
    "Darwin",
    "WINNT_noarch-spidermonkey"
  ],
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

// Matches the OS but since a different entry specifies ABI this doesn't match.
var addon5 = {
  id: "addon5@tests.mozilla.org",
  version: "1.0",
  name: "Test 5",
  targetPlatforms: [
    "XPCShell",
    "XPCShell_foo"
  ],
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

const profileDir = gProfD.clone();
profileDir.append("extensions");

// Set up the profile
async function run_test() {
  do_test_pending();

  writeInstallRDFForExtension(addon1, profileDir);
  writeInstallRDFForExtension(addon2, profileDir);
  writeInstallRDFForExtension(addon3, profileDir);
  writeInstallRDFForExtension(addon4, profileDir);
  writeInstallRDFForExtension(addon5, profileDir);

  await promiseRestartManager();

  AddonManager.getAddonsByIDs(
    ["addon1@tests.mozilla.org",
     "addon2@tests.mozilla.org",
     "addon3@tests.mozilla.org",
     "addon4@tests.mozilla.org",
     "addon5@tests.mozilla.org"],
    function([a1, a2, a3, a4, a5]) {

      do_check_neq(a1, null);
      do_check_false(a1.appDisabled);
      do_check_true(a1.isPlatformCompatible);
      do_check_true(a1.isActive);
      do_check_true(isExtensionInAddonsList(profileDir, a1.id));
      do_check_in_crash_annotation(addon1.id, addon1.version);

      do_check_neq(a2, null);
      do_check_false(a2.appDisabled);
      do_check_true(a2.isPlatformCompatible);
      do_check_true(a2.isActive);
      do_check_true(isExtensionInAddonsList(profileDir, a2.id));
      do_check_in_crash_annotation(addon2.id, addon2.version);

      do_check_neq(a3, null);
      do_check_false(a3.appDisabled);
      do_check_true(a3.isPlatformCompatible);
      do_check_true(a3.isActive);
      do_check_true(isExtensionInAddonsList(profileDir, a3.id));
      do_check_in_crash_annotation(addon3.id, addon3.version);

      do_check_neq(a4, null);
      do_check_true(a4.appDisabled);
      do_check_false(a4.isPlatformCompatible);
      do_check_false(a4.isActive);
      do_check_false(isExtensionInAddonsList(profileDir, a4.id));
      do_check_not_in_crash_annotation(addon4.id, addon4.version);

      do_check_neq(a5, null);
      do_check_true(a5.appDisabled);
      do_check_false(a5.isPlatformCompatible);
      do_check_false(a5.isActive);
      do_check_false(isExtensionInAddonsList(profileDir, a5.id));
      do_check_not_in_crash_annotation(addon5.id, addon5.version);

      do_execute_soon(do_test_finished);
    });
}
