/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/NetUtil.jsm");

// Tests that extensions behave correctly in safe mode

var addon1 = {
  id: "addon1@tests.mozilla.org",
  version: "1.0",
  name: "Test 1",
  optionsURL: "chrome://foo/content/options.xul",
  aboutURL: "chrome://foo/content/about.xul",
  iconURL: "chrome://foo/content/icon.png",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

const profileDir = gProfD.clone();
profileDir.append("extensions");

var gIconURL = null;

// Sets up the profile by installing an add-on.
function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  gAppInfo.inSafeMode = true;

  startupManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", callback_soon(async function(a1) {
    Assert.equal(a1, null);
    do_check_not_in_crash_annotation(addon1.id, addon1.version);

    writeInstallRDFForExtension(addon1, profileDir, addon1.id, "icon.png");
    gIconURL = do_get_addon_root_uri(profileDir.clone(), addon1.id) + "icon.png";

    await promiseRestartManager();

    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(newa1) {
      Assert.notEqual(newa1, null);
      Assert.ok(!newa1.isActive);
      Assert.ok(!newa1.userDisabled);
      Assert.equal(newa1.aboutURL, null);
      Assert.equal(newa1.optionsURL, null);
      Assert.equal(newa1.iconURL, gIconURL);
      Assert.ok(isExtensionInAddonsList(profileDir, newa1.id));
      Assert.ok(hasFlag(newa1.permissions, AddonManager.PERM_CAN_DISABLE));
      Assert.ok(!hasFlag(newa1.permissions, AddonManager.PERM_CAN_ENABLE));
      Assert.equal(newa1.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_NONE);
      do_check_not_in_crash_annotation(addon1.id, addon1.version);

      run_test_1();
    });
  }));
}

// Disabling an add-on should work
function run_test_1() {
  prepare_test({
    "addon1@tests.mozilla.org": [
      ["onDisabling", false],
      "onDisabled"
    ]
  });

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    Assert.ok(!hasFlag(a1.operationsRequiringRestart,
                       AddonManager.OP_NEEDS_RESTART_DISABLE));
    a1.userDisabled = true;
    Assert.ok(!a1.isActive);
    Assert.equal(a1.aboutURL, null);
    Assert.equal(a1.optionsURL, null);
    Assert.equal(a1.iconURL, gIconURL);
    Assert.ok(!hasFlag(a1.permissions, AddonManager.PERM_CAN_DISABLE));
    Assert.ok(hasFlag(a1.permissions, AddonManager.PERM_CAN_ENABLE));
    Assert.equal(a1.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_NONE);
    do_check_not_in_crash_annotation(addon1.id, addon1.version);

    ensure_test_completed();

    run_test_2();
  });
}

// Enabling an add-on should happen without restart but not become active.
function run_test_2() {
  prepare_test({
    "addon1@tests.mozilla.org": [
      ["onEnabling", false],
      "onEnabled"
    ]
  });

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    a1.userDisabled = false;
    Assert.ok(!a1.isActive);
    Assert.equal(a1.aboutURL, null);
    Assert.equal(a1.optionsURL, null);
    Assert.equal(a1.iconURL, gIconURL);
    Assert.ok(hasFlag(a1.permissions, AddonManager.PERM_CAN_DISABLE));
    Assert.ok(!hasFlag(a1.permissions, AddonManager.PERM_CAN_ENABLE));
    Assert.equal(a1.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_NONE);
    do_check_not_in_crash_annotation(addon1.id, addon1.version);

    ensure_test_completed();

    do_execute_soon(do_test_finished);
  });
}
