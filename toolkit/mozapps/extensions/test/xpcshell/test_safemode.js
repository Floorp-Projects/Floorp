/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that extensions behave correctly in safe mode
let scopes = AddonManager.SCOPE_PROFILE | AddonManager.SCOPE_APPLICATION;
Services.prefs.setIntPref("extensions.enabledScopes", scopes);

const profileDir = gProfD.clone();
profileDir.append("extensions");

const ID = "addon1@tests.mozilla.org";
const BUILTIN_ID = "builtin@tests.mozilla.org";
const VERSION = "1.0";

// Sets up the profile by installing an add-on.
add_task(async function setup() {
  AddonTestUtils.createAppInfo(
    "xpcshell@tests.mozilla.org",
    "XPCShell",
    "1",
    "1.9.2"
  );
  gAppInfo.inSafeMode = true;

  await promiseStartupManager();

  let a1 = await AddonManager.getAddonByID(ID);
  Assert.equal(a1, null);
  do_check_not_in_crash_annotation(ID, VERSION);

  await promiseInstallWebExtension({
    manifest: {
      name: "Test 1",
      version: VERSION,
      applications: { gecko: { id: ID } },
    },
  });
  let wrapper = await installBuiltinExtension({
    manifest: {
      applications: { gecko: { id: BUILTIN_ID } },
    },
  });

  let builtin = await AddonManager.getAddonByID(BUILTIN_ID);
  Assert.notEqual(builtin, null, "builtin extension is installed");

  await promiseRestartManager();

  a1 = await AddonManager.getAddonByID(ID);
  Assert.notEqual(a1, null);
  Assert.ok(!a1.isActive);
  Assert.ok(!a1.userDisabled);
  Assert.ok(isExtensionInBootstrappedList(profileDir, ID));
  Assert.ok(hasFlag(a1.permissions, AddonManager.PERM_CAN_DISABLE));
  Assert.ok(!hasFlag(a1.permissions, AddonManager.PERM_CAN_ENABLE));
  do_check_not_in_crash_annotation(ID, VERSION);

  builtin = await AddonManager.getAddonByID(BUILTIN_ID);
  Assert.notEqual(builtin, null, "builtin extension is installed");
  Assert.ok(builtin.isActive, "builtin extension is active");
  await wrapper.unload();
});

// Disabling an add-on should work
add_task(async function test_disable() {
  let a1 = await AddonManager.getAddonByID(ID);
  Assert.ok(
    !hasFlag(
      a1.operationsRequiringRestart,
      AddonManager.OP_NEEDS_RESTART_DISABLE
    )
  );
  await a1.disable();
  Assert.ok(!a1.isActive);
  Assert.ok(!hasFlag(a1.permissions, AddonManager.PERM_CAN_DISABLE));
  Assert.ok(hasFlag(a1.permissions, AddonManager.PERM_CAN_ENABLE));
  do_check_not_in_crash_annotation(ID, VERSION);
});

// Enabling an add-on should happen but not become active.
add_task(async function test_enable() {
  let a1 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  await a1.enable();
  Assert.ok(!a1.isActive);
  Assert.ok(hasFlag(a1.permissions, AddonManager.PERM_CAN_DISABLE));
  Assert.ok(!hasFlag(a1.permissions, AddonManager.PERM_CAN_ENABLE));

  do_check_not_in_crash_annotation(ID, VERSION);
});
