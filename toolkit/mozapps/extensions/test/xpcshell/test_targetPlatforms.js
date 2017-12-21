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

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org",
                               "addon3@tests.mozilla.org",
                               "addon4@tests.mozilla.org",
                               "addon5@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5]) {

    Assert.notEqual(a1, null);
    Assert.ok(!a1.appDisabled);
    Assert.ok(a1.isPlatformCompatible);
    Assert.ok(a1.isActive);
    Assert.ok(isExtensionInAddonsList(profileDir, a1.id));
    do_check_in_crash_annotation(addon1.id, addon1.version);

    Assert.notEqual(a2, null);
    Assert.ok(!a2.appDisabled);
    Assert.ok(a2.isPlatformCompatible);
    Assert.ok(a2.isActive);
    Assert.ok(isExtensionInAddonsList(profileDir, a2.id));
    do_check_in_crash_annotation(addon2.id, addon2.version);

    Assert.notEqual(a3, null);
    Assert.ok(!a3.appDisabled);
    Assert.ok(a3.isPlatformCompatible);
    Assert.ok(a3.isActive);
    Assert.ok(isExtensionInAddonsList(profileDir, a3.id));
    do_check_in_crash_annotation(addon3.id, addon3.version);

    Assert.notEqual(a4, null);
    Assert.ok(a4.appDisabled);
    Assert.ok(!a4.isPlatformCompatible);
    Assert.ok(!a4.isActive);
    Assert.ok(!isExtensionInAddonsList(profileDir, a4.id));
    do_check_not_in_crash_annotation(addon4.id, addon4.version);

    Assert.notEqual(a5, null);
    Assert.ok(a5.appDisabled);
    Assert.ok(!a5.isPlatformCompatible);
    Assert.ok(!a5.isActive);
    Assert.ok(!isExtensionInAddonsList(profileDir, a5.id));
    do_check_not_in_crash_annotation(addon5.id, addon5.version);

    do_execute_soon(do_test_finished);
  });
}
