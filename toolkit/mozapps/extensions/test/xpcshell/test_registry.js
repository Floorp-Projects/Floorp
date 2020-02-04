/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Make Cu.isInAutomation true.
Services.prefs.setBoolPref(
  "security.turn_off_all_security_so_that_viruses_can_take_over_this_computer",
  true
);

// Tests that extensions installed through the registry work as expected
createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

// Enable loading extensions from the user and system scopes
Services.prefs.setIntPref(
  "extensions.enabledScopes",
  AddonManager.SCOPE_PROFILE +
    AddonManager.SCOPE_USER +
    AddonManager.SCOPE_SYSTEM
);

Services.prefs.setIntPref("extensions.sideloadScopes", AddonManager.SCOPE_ALL);

const ID1 = "addon1@tests.mozilla.org";
const ID2 = "addon2@tests.mozilla.org";
let xpi1, xpi2;

let registry;

add_task(async function setup() {
  xpi1 = await createTempWebExtensionFile({
    manifest: { applications: { gecko: { id: ID1 } } },
  });

  xpi2 = await createTempWebExtensionFile({
    manifest: { applications: { gecko: { id: ID2 } } },
  });

  registry = new MockRegistry();
  registerCleanupFunction(() => {
    registry.shutdown();
  });
});

// Tests whether basic registry install works
add_task(async function test_1() {
  registry.setValue(
    Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
    ID1,
    xpi1.path
  );
  registry.setValue(
    Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
    ID2,
    xpi2.path
  );

  await promiseStartupManager();

  let [a1, a2] = await AddonManager.getAddonsByIDs([ID1, ID2]);
  notEqual(a1, null);
  ok(a1.isActive);
  ok(!hasFlag(a1.permissions, AddonManager.PERM_CAN_UNINSTALL));
  equal(a1.scope, AddonManager.SCOPE_SYSTEM);

  notEqual(a2, null);
  ok(a2.isActive);
  ok(!hasFlag(a2.permissions, AddonManager.PERM_CAN_UNINSTALL));
  equal(a2.scope, AddonManager.SCOPE_USER);
});

// Tests whether uninstalling from the registry works
add_task(async function test_2() {
  registry.setValue(
    Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
    ID1,
    null
  );
  registry.setValue(
    Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
    ID2,
    null
  );

  await promiseRestartManager();

  let [a1, a2] = await AddonManager.getAddonsByIDs([ID1, ID2]);
  equal(a1, null);
  equal(a2, null);
});

// Checks that the ID in the registry must match that in the install manifest
add_task(async function test_3() {
  registry.setValue(
    Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
    ID1,
    xpi2.path
  );
  registry.setValue(
    Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
    ID2,
    xpi1.path
  );

  await promiseRestartManager();

  let [a1, a2] = await AddonManager.getAddonsByIDs([ID1, ID2]);
  equal(a1, null);
  equal(a2, null);
});

// Tests whether an extension's ID can change without its directory changing
add_task(async function test_4() {
  // Restarting with bad items in the registry should not force an EM restart
  await promiseRestartManager();

  registry.setValue(
    Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
    ID1,
    null
  );
  registry.setValue(
    Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
    ID2,
    null
  );

  await promiseRestartManager();

  registry.setValue(
    Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
    ID1,
    xpi1.path
  );

  await promiseShutdownManager();

  registry.setValue(
    Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
    ID1,
    null
  );
  registry.setValue(
    Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
    "SOFTWARE\\Mozilla\\XPCShell\\Extensions",
    ID2,
    xpi1.path
  );
  xpi2.copyTo(xpi1.parent, xpi1.leafName);

  await promiseStartupManager();

  let [a1, a2] = await AddonManager.getAddonsByIDs([ID1, ID2]);
  equal(a1, null);
  notEqual(a2, null);
});
