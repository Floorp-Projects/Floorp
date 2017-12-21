/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that extensions installed through the registry work as expected
createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

// Enable loading extensions from the user and system scopes
Services.prefs.setIntPref("extensions.enabledScopes",
                          AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_USER +
                          AddonManager.SCOPE_SYSTEM);

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
    maxVersion: "2"
  }]
};

const addon1Dir = writeInstallRDFForExtension(addon1, gProfD, "addon1");
const addon2Dir = writeInstallRDFForExtension(addon2, gProfD, "addon2");

let registry;

function run_test() {
  // This test only works where there is a registry.
  if (!("nsIWindowsRegKey" in AM_Ci))
    return;

  registry = new MockRegistry();
  registerCleanupFunction(() => {
    registry.shutdown();
  });

  do_test_pending();

  run_test_1();
}

// Tests whether basic registry install works
function run_test_1() {
  registry.setValue(AM_Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
                   "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon1@tests.mozilla.org", addon1Dir.path);
  registry.setValue(AM_Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon2@tests.mozilla.org", addon2Dir.path);

  startupManager();

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org"], function([a1, a2]) {
    Assert.notEqual(a1, null);
    Assert.ok(a1.isActive);
    Assert.ok(!hasFlag(a1.permissions, AddonManager.PERM_CAN_UNINSTALL));
    Assert.equal(a1.scope, AddonManager.SCOPE_SYSTEM);

    Assert.notEqual(a2, null);
    Assert.ok(a2.isActive);
    Assert.ok(!hasFlag(a2.permissions, AddonManager.PERM_CAN_UNINSTALL));
    Assert.equal(a2.scope, AddonManager.SCOPE_USER);

    executeSoon(run_test_2);
  });
}

// Tests whether uninstalling from the registry works
function run_test_2() {
  registry.setValue(AM_Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon1@tests.mozilla.org", null);
  registry.setValue(AM_Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon2@tests.mozilla.org", null);

  restartManager();

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org"], function([a1, a2]) {
    Assert.equal(a1, null);
    Assert.equal(a2, null);

    executeSoon(run_test_3);
  });
}

// Checks that the ID in the registry must match that in the install manifest
function run_test_3() {
  registry.setValue(AM_Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon1@tests.mozilla.org", addon2Dir.path);
  registry.setValue(AM_Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon2@tests.mozilla.org", addon1Dir.path);

  restartManager();

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org"], function([a1, a2]) {
    Assert.equal(a1, null);
    Assert.equal(a2, null);

    executeSoon(run_test_4);
  });
}

// Tests whether an extension's ID can change without its directory changing
function run_test_4() {
  // Restarting with bad items in the registry should not force an EM restart
  restartManager();

  registry.setValue(AM_Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon1@tests.mozilla.org", null);
  registry.setValue(AM_Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon2@tests.mozilla.org", null);

  restartManager();

  registry.setValue(AM_Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon1@tests.mozilla.org", addon1Dir.path);
  restartManager();

  registry.setValue(AM_Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon1@tests.mozilla.org", null);
  registry.setValue(AM_Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon2@tests.mozilla.org", addon1Dir.path);
  writeInstallRDFForExtension(addon2, gProfD, "addon1");

  restartManager();

  AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                               "addon2@tests.mozilla.org"], function([a1, a2]) {
    Assert.equal(a1, null);
    Assert.notEqual(a2, null);

    executeSoon(do_test_finished);
  });
}
