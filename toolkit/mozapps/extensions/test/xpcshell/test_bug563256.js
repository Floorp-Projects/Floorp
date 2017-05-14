/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that the themes switch as expected

const PREF_GENERAL_SKINS_SELECTEDSKIN = "general.skins.selectedSkin";

const profileDir = gProfD.clone();
profileDir.append("extensions");

async function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  writeInstallRDFForExtension({
    id: "default@tests.mozilla.org",
    version: "1.0",
    name: "Default",
    internalName: "classic/1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "2"
    }]
  }, profileDir);

  writeInstallRDFForExtension({
    id: "alternate@tests.mozilla.org",
    version: "1.0",
    name: "Test 1",
    type: 4,
    internalName: "alternate/1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "2"
    }]
  }, profileDir);

  await promiseStartupManager();

  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");

  AddonManager.getAddonsByIDs(["default@tests.mozilla.org",
                               "alternate@tests.mozilla.org"], function([d, a]) {
    do_check_neq(d, null);
    do_check_false(d.userDisabled);
    do_check_false(d.appDisabled);
    do_check_true(d.isActive);
    do_check_true(isThemeInAddonsList(profileDir, d.id));
    do_check_false(hasFlag(d.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_false(hasFlag(d.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_neq(a, null);
    do_check_true(a.userDisabled);
    do_check_false(a.appDisabled);
    do_check_false(a.isActive);
    do_check_false(isThemeInAddonsList(profileDir, a.id));
    do_check_false(hasFlag(a.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_true(hasFlag(a.permissions, AddonManager.PERM_CAN_ENABLE));

    run_test_1(d, a);
  });
}

function end_test() {
  do_execute_soon(do_test_finished);
}

// Checks switching to a different theme and back again leaves everything the
// same
function run_test_1(d, a) {
  a.userDisabled = false;

  do_check_true(d.userDisabled);
  do_check_false(d.appDisabled);
  do_check_true(d.isActive);
  do_check_true(isThemeInAddonsList(profileDir, d.id));
  do_check_false(hasFlag(d.permissions, AddonManager.PERM_CAN_DISABLE));
  do_check_true(hasFlag(d.permissions, AddonManager.PERM_CAN_ENABLE));

  do_check_false(a.userDisabled);
  do_check_false(a.appDisabled);
  do_check_false(a.isActive);
  do_check_false(isThemeInAddonsList(profileDir, a.id));
  do_check_false(hasFlag(a.permissions, AddonManager.PERM_CAN_DISABLE));
  do_check_false(hasFlag(a.permissions, AddonManager.PERM_CAN_ENABLE));

  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");

  d.userDisabled = false;

  do_check_false(d.userDisabled);
  do_check_false(d.appDisabled);
  do_check_true(d.isActive);
  do_check_true(isThemeInAddonsList(profileDir, d.id));
  do_check_false(hasFlag(d.permissions, AddonManager.PERM_CAN_DISABLE));
  do_check_false(hasFlag(d.permissions, AddonManager.PERM_CAN_ENABLE));

  do_check_true(a.userDisabled);
  do_check_false(a.appDisabled);
  do_check_false(a.isActive);
  do_check_false(isThemeInAddonsList(profileDir, a.id));
  do_check_false(hasFlag(a.permissions, AddonManager.PERM_CAN_DISABLE));
  do_check_true(hasFlag(a.permissions, AddonManager.PERM_CAN_ENABLE));

  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");

  do_execute_soon(run_test_2);
}

// Tests that after the restart themes can be changed as expected
function run_test_2() {
  restartManager();
  AddonManager.getAddonsByIDs(["default@tests.mozilla.org",
                               "alternate@tests.mozilla.org"], function([d, a]) {
    do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");

    do_check_neq(d, null);
    do_check_false(d.userDisabled);
    do_check_false(d.appDisabled);
    do_check_true(d.isActive);
    do_check_true(isThemeInAddonsList(profileDir, d.id));
    do_check_false(hasFlag(d.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_false(hasFlag(d.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_neq(a, null);
    do_check_true(a.userDisabled);
    do_check_false(a.appDisabled);
    do_check_false(a.isActive);
    do_check_false(isThemeInAddonsList(profileDir, a.id));
    do_check_false(hasFlag(a.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_true(hasFlag(a.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");

    a.userDisabled = false;

    do_check_true(d.userDisabled);
    do_check_false(d.appDisabled);
    do_check_true(d.isActive);
    do_check_true(isThemeInAddonsList(profileDir, d.id));
    do_check_false(hasFlag(d.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_true(hasFlag(d.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_false(a.userDisabled);
    do_check_false(a.appDisabled);
    do_check_false(a.isActive);
    do_check_false(isThemeInAddonsList(profileDir, a.id));
    do_check_false(hasFlag(a.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_false(hasFlag(a.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");

    d.userDisabled = false;

    do_check_false(d.userDisabled);
    do_check_false(d.appDisabled);
    do_check_true(d.isActive);
    do_check_true(isThemeInAddonsList(profileDir, d.id));
    do_check_false(hasFlag(d.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_false(hasFlag(d.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_true(a.userDisabled);
    do_check_false(a.appDisabled);
    do_check_false(a.isActive);
    do_check_false(isThemeInAddonsList(profileDir, a.id));
    do_check_false(hasFlag(a.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_true(hasFlag(a.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");

    a.userDisabled = false;

    do_check_true(d.userDisabled);
    do_check_false(d.appDisabled);
    do_check_true(d.isActive);
    do_check_true(isThemeInAddonsList(profileDir, d.id));
    do_check_false(hasFlag(d.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_true(hasFlag(d.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_false(a.userDisabled);
    do_check_false(a.appDisabled);
    do_check_false(a.isActive);
    do_check_false(isThemeInAddonsList(profileDir, a.id));
    do_check_false(hasFlag(a.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_false(hasFlag(a.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");

    do_execute_soon(check_test_2);
  });
}

function check_test_2() {
  restartManager();
  AddonManager.getAddonsByIDs(["default@tests.mozilla.org",
                               "alternate@tests.mozilla.org"], callback_soon(function([d, a]) {
    do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "alternate/1.0");

    do_check_true(d.userDisabled);
    do_check_false(d.appDisabled);
    do_check_false(d.isActive);
    do_check_false(isThemeInAddonsList(profileDir, d.id));
    do_check_false(hasFlag(d.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_true(hasFlag(d.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_false(a.userDisabled);
    do_check_false(a.appDisabled);
    do_check_true(a.isActive);
    do_check_true(isThemeInAddonsList(profileDir, a.id));
    do_check_false(hasFlag(a.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_false(hasFlag(a.permissions, AddonManager.PERM_CAN_ENABLE));

    d.userDisabled = false;

    do_check_false(d.userDisabled);
    do_check_false(d.appDisabled);
    do_check_false(d.isActive);
    do_check_false(isThemeInAddonsList(profileDir, d.id));
    do_check_false(hasFlag(d.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_false(hasFlag(d.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_true(a.userDisabled);
    do_check_false(a.appDisabled);
    do_check_true(a.isActive);
    do_check_true(isThemeInAddonsList(profileDir, a.id));
    do_check_false(hasFlag(a.permissions, AddonManager.PERM_CAN_DISABLE));
    do_check_true(hasFlag(a.permissions, AddonManager.PERM_CAN_ENABLE));

    do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "alternate/1.0");

    restartManager();

    do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");

    AddonManager.getAddonsByIDs(["default@tests.mozilla.org",
                                 "alternate@tests.mozilla.org"], function([d2, a2]) {
      do_check_neq(d2, null);
      do_check_false(d2.userDisabled);
      do_check_false(d2.appDisabled);
      do_check_true(d2.isActive);
      do_check_true(isThemeInAddonsList(profileDir, d2.id));
      do_check_false(hasFlag(d2.permissions, AddonManager.PERM_CAN_DISABLE));
      do_check_false(hasFlag(d2.permissions, AddonManager.PERM_CAN_ENABLE));

      do_check_neq(a2, null);
      do_check_true(a2.userDisabled);
      do_check_false(a2.appDisabled);
      do_check_false(a2.isActive);
      do_check_false(isThemeInAddonsList(profileDir, a2.id));
      do_check_false(hasFlag(a2.permissions, AddonManager.PERM_CAN_DISABLE));
      do_check_true(hasFlag(a2.permissions, AddonManager.PERM_CAN_ENABLE));

      end_test();
    });
  }));
}
