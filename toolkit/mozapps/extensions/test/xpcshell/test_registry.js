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
  bootstrap: true,
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
  bootstrap: true,
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "2"
  }]
};

const IDS = ["addon1@tests.mozilla.org",
             "addon2@tests.mozilla.org",
             "addon3@tests.mozilla.org"];


var addon1Dir;
var addon2Dir;
const addon3Dir = gProfD.clone();
addon3Dir.append("addon3@tests.mozilla.org");

let registry;

add_task(async function setup() {
  addon1Dir = await promiseWriteInstallRDFForExtension(addon1, gProfD, "addon1");
  addon2Dir = await promiseWriteInstallRDFForExtension(addon2, gProfD, "addon2");

  registry = new MockRegistry();
  registerCleanupFunction(() => {
    registry.shutdown();
  });
});

// Tests whether basic registry install works
add_task(async function test_1() {
  registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
                   "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon1@tests.mozilla.org", addon1Dir.path);
  registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon2@tests.mozilla.org", addon2Dir.path);
  registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon3@tests.mozilla.org", addon3Dir.path);

  await promiseStartupManager();

  let [a1, a2, a3] = await AddonManager.getAddonsByIDs(IDS);
  notEqual(a1, null);
  ok(a1.isActive);
  ok(!hasFlag(a1.permissions, AddonManager.PERM_CAN_UNINSTALL));
  equal(a1.scope, AddonManager.SCOPE_SYSTEM);

  notEqual(a2, null);
  ok(a2.isActive);
  ok(!hasFlag(a2.permissions, AddonManager.PERM_CAN_UNINSTALL));
  equal(a2.scope, AddonManager.SCOPE_USER);

  equal(a3, null);
});

// Tests whether uninstalling from the registry works
add_task(async function test_2() {
  registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon1@tests.mozilla.org", null);
  registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon2@tests.mozilla.org", null);
  registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon3@tests.mozilla.org", null);

  await promiseRestartManager();

  let [a1, a2, a3] = await AddonManager.getAddonsByIDs(IDS);
  equal(a1, null);
  equal(a2, null);
  equal(a3, null);
});

// Checks that the ID in the registry must match that in the install manifest
add_task(async function test_3() {
  registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon1@tests.mozilla.org", addon2Dir.path);
  registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon2@tests.mozilla.org", addon1Dir.path);
  registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon3@tests.mozilla.org", addon3Dir.path);

  await promiseRestartManager();

  let [a1, a2, a3] = await AddonManager.getAddonsByIDs(IDS);
  equal(a1, null);
  equal(a2, null);
  equal(a3, null);
});

// Tests whether an extension's ID can change without its directory changing
add_task(async function test_4() {
  // Restarting with bad items in the registry should not force an EM restart
  await promiseRestartManager();

  registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon1@tests.mozilla.org", null);
  registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon2@tests.mozilla.org", null);
  registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon3@tests.mozilla.org", null);

  await promiseRestartManager();

  registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon1@tests.mozilla.org", addon1Dir.path);
  await promiseRestartManager();

  registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon1@tests.mozilla.org", null);
  registry.setValue(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
                    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
                    "addon2@tests.mozilla.org", addon1Dir.path);
  await promiseWriteInstallRDFForExtension(addon2, gProfD, "addon1");

  await promiseRestartManager();

  let [a1, a2, a3] = await AddonManager.getAddonsByIDs(IDS);
  equal(a1, null);
  notEqual(a2, null);
  equal(a3, null);
});
