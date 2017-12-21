/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/NetUtil.jsm");

// This verifies that add-ons can be disabled and enabled.

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

  startupManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", callback_soon(async function(a1) {
    Assert.equal(a1, null);
    do_check_not_in_crash_annotation(addon1.id, addon1.version);

    writeInstallRDFForExtension(addon1, profileDir, addon1.id, "icon.png");
    gIconURL = do_get_addon_root_uri(profileDir.clone(), addon1.id) + "icon.png";

    await promiseRestartManager();

    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(newa1) {
      Assert.notEqual(newa1, null);
      Assert.ok(newa1.isActive);
      Assert.ok(!newa1.userDisabled);
      Assert.equal(newa1.aboutURL, "chrome://foo/content/about.xul");
      Assert.equal(newa1.optionsURL, "chrome://foo/content/options.xul");
      Assert.equal(newa1.iconURL, "chrome://foo/content/icon.png");
      Assert.ok(isExtensionInAddonsList(profileDir, newa1.id));
      Assert.ok(hasFlag(newa1.permissions, AddonManager.PERM_CAN_DISABLE));
      Assert.ok(!hasFlag(newa1.permissions, AddonManager.PERM_CAN_ENABLE));
      Assert.equal(newa1.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_DISABLE |
                                                     AddonManager.OP_NEEDS_RESTART_UNINSTALL);
      do_check_in_crash_annotation(addon1.id, addon1.version);

      run_test_1();
    });
  }));
}

// Disabling an add-on should work
function run_test_1() {
  prepare_test({
    "addon1@tests.mozilla.org": [
      "onDisabling"
    ]
  });

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    Assert.notEqual(a1.operationsRequiringRestart &
                    AddonManager.OP_NEEDS_RESTART_DISABLE, 0);
    a1.userDisabled = true;
    Assert.equal(a1.aboutURL, "chrome://foo/content/about.xul");
    Assert.equal(a1.optionsURL, "chrome://foo/content/options.xul");
    Assert.equal(a1.iconURL, "chrome://foo/content/icon.png");
    Assert.ok(!hasFlag(a1.permissions, AddonManager.PERM_CAN_DISABLE));
    Assert.ok(hasFlag(a1.permissions, AddonManager.PERM_CAN_ENABLE));
    Assert.equal(a1.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_DISABLE |
                                                AddonManager.OP_NEEDS_RESTART_UNINSTALL);
    do_check_in_crash_annotation(addon1.id, addon1.version);

    ensure_test_completed();

    AddonManager.getAddonsWithOperationsByTypes(null, callback_soon(function(list) {
      Assert.equal(list.length, 1);
      Assert.equal(list[0].id, "addon1@tests.mozilla.org");

      restartManager();

      AddonManager.getAddonByID("addon1@tests.mozilla.org", function(newa1) {
        Assert.notEqual(newa1, null);
        Assert.ok(!newa1.isActive);
        Assert.ok(newa1.userDisabled);
        Assert.equal(newa1.aboutURL, null);
        Assert.equal(newa1.optionsURL, null);
        Assert.equal(newa1.iconURL, gIconURL);
        Assert.ok(!isExtensionInAddonsList(profileDir, newa1.id));
        Assert.ok(!hasFlag(newa1.permissions, AddonManager.PERM_CAN_DISABLE));
        Assert.ok(hasFlag(newa1.permissions, AddonManager.PERM_CAN_ENABLE));
        Assert.equal(newa1.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_ENABLE);
        do_check_not_in_crash_annotation(addon1.id, addon1.version);

        run_test_2();
      });
    }));
  });
}

// Enabling an add-on should work.
function run_test_2() {
  prepare_test({
    "addon1@tests.mozilla.org": [
      "onEnabling"
    ]
  });

  AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
    a1.userDisabled = false;
    Assert.equal(a1.aboutURL, null);
    Assert.equal(a1.optionsURL, null);
    Assert.equal(a1.iconURL, gIconURL);
    Assert.ok(hasFlag(a1.permissions, AddonManager.PERM_CAN_DISABLE));
    Assert.ok(!hasFlag(a1.permissions, AddonManager.PERM_CAN_ENABLE));
    Assert.equal(a1.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_ENABLE);

    ensure_test_completed();

    AddonManager.getAddonsWithOperationsByTypes(null, callback_soon(function(list) {
      Assert.equal(list.length, 1);
      Assert.equal(list[0].id, "addon1@tests.mozilla.org");

      restartManager();

      AddonManager.getAddonByID("addon1@tests.mozilla.org", function(newa1) {
        Assert.notEqual(newa1, null);
        Assert.ok(newa1.isActive);
        Assert.ok(!newa1.userDisabled);
        Assert.equal(newa1.aboutURL, "chrome://foo/content/about.xul");
        Assert.equal(newa1.optionsURL, "chrome://foo/content/options.xul");
        Assert.equal(newa1.iconURL, "chrome://foo/content/icon.png");
        Assert.ok(isExtensionInAddonsList(profileDir, newa1.id));
        Assert.ok(hasFlag(newa1.permissions, AddonManager.PERM_CAN_DISABLE));
        Assert.ok(!hasFlag(newa1.permissions, AddonManager.PERM_CAN_ENABLE));
        Assert.equal(newa1.operationsRequiringRestart, AddonManager.OP_NEEDS_RESTART_DISABLE |
                                                       AddonManager.OP_NEEDS_RESTART_UNINSTALL);
        do_check_in_crash_annotation(addon1.id, addon1.version);

        run_test_3();
      });
    }));
  });
}

// Disabling then enabling without restart should fire onOperationCancelled.
function run_test_3() {
  prepare_test({
    "addon1@tests.mozilla.org": [
      "onDisabling"
    ]
  });

  AddonManager.getAddonByID("addon1@tests.mozilla.org", callback_soon(function(a1) {
    a1.userDisabled = true;
    ensure_test_completed();
    prepare_test({
      "addon1@tests.mozilla.org": [
        "onOperationCancelled"
      ]
    });
    a1.userDisabled = false;
    Assert.ok(hasFlag(a1.permissions, AddonManager.PERM_CAN_DISABLE));
    Assert.ok(!hasFlag(a1.permissions, AddonManager.PERM_CAN_ENABLE));

    ensure_test_completed();

    restartManager();

    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(newa1) {
      Assert.notEqual(newa1, null);
      Assert.ok(newa1.isActive);
      Assert.ok(!newa1.userDisabled);
      Assert.equal(newa1.aboutURL, "chrome://foo/content/about.xul");
      Assert.equal(newa1.optionsURL, "chrome://foo/content/options.xul");
      Assert.equal(newa1.iconURL, "chrome://foo/content/icon.png");
      Assert.ok(isExtensionInAddonsList(profileDir, newa1.id));
      Assert.ok(hasFlag(newa1.permissions, AddonManager.PERM_CAN_DISABLE));
      Assert.ok(!hasFlag(newa1.permissions, AddonManager.PERM_CAN_ENABLE));

      executeSoon(do_test_finished);
    });
  }));
}
