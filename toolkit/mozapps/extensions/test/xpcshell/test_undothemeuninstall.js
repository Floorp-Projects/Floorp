/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that forcing undo for uninstall works for themes
Components.utils.import("resource://gre/modules/LightweightThemeManager.jsm");

const PREF_GENERAL_SKINS_SELECTEDSKIN = "general.skins.selectedSkin";

var defaultTheme = {
  id: "default@tests.mozilla.org",
  version: "1.0",
  name: "Test 1",
  internalName: "classic/1.0",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

var theme1 = {
  id: "theme1@tests.mozilla.org",
  version: "1.0",
  name: "Test 1",
  internalName: "theme1",
  targetApplications: [{
    id: "xpcshell@tests.mozilla.org",
    minVersion: "1",
    maxVersion: "1"
  }]
};

const profileDir = gProfD.clone();
profileDir.append("extensions");

function dummyLWTheme(id) {
  return {
    id: id || Math.random().toString(),
    name: Math.random().toString(),
    headerURL: "http://lwttest.invalid/a.png",
    footerURL: "http://lwttest.invalid/b.png",
    textcolor: Math.random().toString(),
    accentcolor: Math.random().toString()
  };
}

// Sets up the profile by installing an add-on.
function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  startupManager();
  do_register_cleanup(promiseShutdownManager);

  run_next_test();
}

add_task(function* checkDefault() {
  writeInstallRDFForExtension(defaultTheme, profileDir);
  yield promiseRestartManager();

  let d = yield promiseAddonByID("default@tests.mozilla.org");

  do_check_neq(d, null);
  do_check_true(d.isActive);
  do_check_false(d.userDisabled);
  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");
});

// Tests that uninstalling an enabled theme offers the option to undo
add_task(function* uninstallEnabledOffersUndo() {
  writeInstallRDFForExtension(theme1, profileDir);

  yield promiseRestartManager();

  let t1 = yield promiseAddonByID("theme1@tests.mozilla.org");

  do_check_neq(t1, null);
  do_check_true(t1.userDisabled);

  t1.userDisabled = false;

  yield promiseRestartManager();

  let d = null;
  [ t1, d ] = yield promiseAddonsByIDs(["theme1@tests.mozilla.org",
                                        "default@tests.mozilla.org"]);
  do_check_neq(d, null);
  do_check_false(d.isActive);
  do_check_true(d.userDisabled);
  do_check_eq(d.pendingOperations, AddonManager.PENDING_NONE);

  do_check_neq(t1, null);
  do_check_true(t1.isActive);
  do_check_false(t1.userDisabled);
  do_check_eq(t1.pendingOperations, AddonManager.PENDING_NONE);

  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "theme1");

  prepare_test({
    "default@tests.mozilla.org": [
      "onEnabling"
    ],
    "theme1@tests.mozilla.org": [
      "onUninstalling"
    ]
  });
  t1.uninstall(true);
  ensure_test_completed();

  do_check_neq(d, null);
  do_check_false(d.isActive);
  do_check_false(d.userDisabled);
  do_check_eq(d.pendingOperations, AddonManager.PENDING_ENABLE);

  do_check_true(t1.isActive);
  do_check_false(t1.userDisabled);
  do_check_true(hasFlag(t1.pendingOperations, AddonManager.PENDING_UNINSTALL));

  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "theme1");

  yield promiseRestartManager();

  [ t1, d ] = yield promiseAddonsByIDs(["theme1@tests.mozilla.org",
                                        "default@tests.mozilla.org"]);
  do_check_neq(d, null);
  do_check_true(d.isActive);
  do_check_false(d.userDisabled);
  do_check_eq(d.pendingOperations, AddonManager.PENDING_NONE);

  do_check_eq(t1, null);

  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");
});

// Tests that uninstalling an enabled theme can be undone
add_task(function* canUndoUninstallEnabled() {
  writeInstallRDFForExtension(theme1, profileDir);

  yield promiseRestartManager();

  let t1 = yield promiseAddonByID("theme1@tests.mozilla.org");

  do_check_neq(t1, null);
  do_check_true(t1.userDisabled);

  t1.userDisabled = false;

  yield promiseRestartManager();

  let d = null;
  [ t1, d ] = yield promiseAddonsByIDs(["theme1@tests.mozilla.org",
                                        "default@tests.mozilla.org"]);

  do_check_neq(d, null);
  do_check_false(d.isActive);
  do_check_true(d.userDisabled);
  do_check_eq(d.pendingOperations, AddonManager.PENDING_NONE);

  do_check_neq(t1, null);
  do_check_true(t1.isActive);
  do_check_false(t1.userDisabled);
  do_check_eq(t1.pendingOperations, AddonManager.PENDING_NONE);

  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "theme1");

  prepare_test({
    "default@tests.mozilla.org": [
      "onEnabling"
    ],
    "theme1@tests.mozilla.org": [
      "onUninstalling"
    ]
  });
  t1.uninstall(true);
  ensure_test_completed();

  do_check_neq(d, null);
  do_check_false(d.isActive);
  do_check_false(d.userDisabled);
  do_check_eq(d.pendingOperations, AddonManager.PENDING_ENABLE);

  do_check_true(t1.isActive);
  do_check_false(t1.userDisabled);
  do_check_true(hasFlag(t1.pendingOperations, AddonManager.PENDING_UNINSTALL));

  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "theme1");

  prepare_test({
    "default@tests.mozilla.org": [
      "onOperationCancelled"
    ],
    "theme1@tests.mozilla.org": [
      "onOperationCancelled"
    ]
  });
  t1.cancelUninstall();
  ensure_test_completed();

  do_check_neq(d, null);
  do_check_false(d.isActive);
  do_check_true(d.userDisabled);
  do_check_eq(d.pendingOperations, AddonManager.PENDING_NONE);

  do_check_neq(t1, null);
  do_check_true(t1.isActive);
  do_check_false(t1.userDisabled);
  do_check_eq(t1.pendingOperations, AddonManager.PENDING_NONE);

  yield promiseRestartManager();

  [ t1, d ] = yield promiseAddonsByIDs(["theme1@tests.mozilla.org",
                                        "default@tests.mozilla.org"]);

  do_check_neq(d, null);
  do_check_false(d.isActive);
  do_check_true(d.userDisabled);
  do_check_eq(d.pendingOperations, AddonManager.PENDING_NONE);

  do_check_neq(t1, null);
  do_check_true(t1.isActive);
  do_check_false(t1.userDisabled);
  do_check_eq(t1.pendingOperations, AddonManager.PENDING_NONE);

  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "theme1");

  t1.uninstall();
  yield promiseRestartManager();
});

// Tests that uninstalling a disabled theme offers the option to undo
add_task(function* uninstallDisabledOffersUndo() {
  writeInstallRDFForExtension(theme1, profileDir);

  yield promiseRestartManager();

  let [ t1, d ] = yield promiseAddonsByIDs(["theme1@tests.mozilla.org",
                                            "default@tests.mozilla.org"]);

  do_check_neq(d, null);
  do_check_true(d.isActive);
  do_check_false(d.userDisabled);
  do_check_eq(d.pendingOperations, AddonManager.PENDING_NONE);

  do_check_neq(t1, null);
  do_check_false(t1.isActive);
  do_check_true(t1.userDisabled);
  do_check_eq(t1.pendingOperations, AddonManager.PENDING_NONE);

  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");

  prepare_test({
    "theme1@tests.mozilla.org": [
      "onUninstalling"
    ]
  });
  t1.uninstall(true);
  ensure_test_completed();

  do_check_neq(d, null);
  do_check_true(d.isActive);
  do_check_false(d.userDisabled);
  do_check_eq(d.pendingOperations, AddonManager.PENDING_NONE);

  do_check_false(t1.isActive);
  do_check_true(t1.userDisabled);
  do_check_true(hasFlag(t1.pendingOperations, AddonManager.PENDING_UNINSTALL));

  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");

  yield promiseRestartManager();

  [ t1, d ] = yield promiseAddonsByIDs(["theme1@tests.mozilla.org",
                                        "default@tests.mozilla.org"]);

  do_check_neq(d, null);
  do_check_true(d.isActive);
  do_check_false(d.userDisabled);
  do_check_eq(d.pendingOperations, AddonManager.PENDING_NONE);

  do_check_eq(t1, null);

  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");
});

// Tests that uninstalling a disabled theme can be undone
add_task(function* canUndoUninstallDisabled() {
  writeInstallRDFForExtension(theme1, profileDir);

  yield promiseRestartManager();

  let [ t1, d ] = yield promiseAddonsByIDs(["theme1@tests.mozilla.org",
                                            "default@tests.mozilla.org"]);

  do_check_neq(d, null);
  do_check_true(d.isActive);
  do_check_false(d.userDisabled);
  do_check_eq(d.pendingOperations, AddonManager.PENDING_NONE);

  do_check_neq(t1, null);
  do_check_false(t1.isActive);
  do_check_true(t1.userDisabled);
  do_check_eq(t1.pendingOperations, AddonManager.PENDING_NONE);

  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");

  prepare_test({
    "theme1@tests.mozilla.org": [
      "onUninstalling"
    ]
  });
  t1.uninstall(true);
  ensure_test_completed();

  do_check_neq(d, null);
  do_check_true(d.isActive);
  do_check_false(d.userDisabled);
  do_check_eq(d.pendingOperations, AddonManager.PENDING_NONE);

  do_check_false(t1.isActive);
  do_check_true(t1.userDisabled);
  do_check_true(hasFlag(t1.pendingOperations, AddonManager.PENDING_UNINSTALL));

  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");

  prepare_test({
    "theme1@tests.mozilla.org": [
      "onOperationCancelled"
    ]
  });
  t1.cancelUninstall();
  ensure_test_completed();

  do_check_neq(d, null);
  do_check_true(d.isActive);
  do_check_false(d.userDisabled);
  do_check_eq(d.pendingOperations, AddonManager.PENDING_NONE);

  do_check_neq(t1, null);
  do_check_false(t1.isActive);
  do_check_true(t1.userDisabled);
  do_check_eq(t1.pendingOperations, AddonManager.PENDING_NONE);

  yield promiseRestartManager();

  [ t1, d ] = yield promiseAddonsByIDs(["theme1@tests.mozilla.org",
                                        "default@tests.mozilla.org"]);

  do_check_neq(d, null);
  do_check_true(d.isActive);
  do_check_false(d.userDisabled);
  do_check_eq(d.pendingOperations, AddonManager.PENDING_NONE);

  do_check_neq(t1, null);
  do_check_false(t1.isActive);
  do_check_true(t1.userDisabled);
  do_check_eq(t1.pendingOperations, AddonManager.PENDING_NONE);

  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");

  t1.uninstall();
  yield promiseRestartManager();
});

// Tests that uninstalling an enabled lightweight theme offers the option to undo
add_task(function* uninstallLWTOffersUndo() {
  // skipped since lightweight themes don't support undoable uninstall yet
  return;
  LightweightThemeManager.currentTheme = dummyLWTheme("theme1");

  let [ t1, d ] = yield promiseAddonsByIDs(["theme1@personas.mozilla.org",
                                            "default@tests.mozilla.org"]);

  do_check_neq(d, null);
  do_check_false(d.isActive);
  do_check_true(d.userDisabled);
  do_check_eq(d.pendingOperations, AddonManager.PENDING_NONE);

  do_check_neq(t1, null);
  do_check_true(t1.isActive);
  do_check_false(t1.userDisabled);
  do_check_eq(t1.pendingOperations, AddonManager.PENDING_NONE);

  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");

  prepare_test({
    "default@tests.mozilla.org": [
      "onEnabling"
    ],
    "theme1@personas.mozilla.org": [
      "onUninstalling"
    ]
  });
  t1.uninstall(true);
  ensure_test_completed();

  do_check_neq(d, null);
  do_check_false(d.isActive);
  do_check_false(d.userDisabled);
  do_check_eq(d.pendingOperations, AddonManager.PENDING_ENABLE);

  do_check_true(t1.isActive);
  do_check_false(t1.userDisabled);
  do_check_true(hasFlag(t1.pendingOperations, AddonManager.PENDING_UNINSTALL));

  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");

  yield promiseRestartManager();

  [ t1, d ] = yield promiseAddonsByIDs(["theme1@personas.mozilla.org",
                                        "default@tests.mozilla.org"]);

  do_check_neq(d, null);
  do_check_true(d.isActive);
  do_check_false(d.userDisabled);
  do_check_eq(d.pendingOperations, AddonManager.PENDING_NONE);

  do_check_eq(t1, null);

  do_check_eq(Services.prefs.getCharPref(PREF_GENERAL_SKINS_SELECTEDSKIN), "classic/1.0");
});
