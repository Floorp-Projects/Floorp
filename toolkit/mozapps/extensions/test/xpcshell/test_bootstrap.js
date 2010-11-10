/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const APP_STARTUP                     = 1;
const APP_SHUTDOWN                    = 2;
const ADDON_ENABLE                    = 3;
const ADDON_DISABLE                   = 4;
const ADDON_INSTALL                   = 5;
const ADDON_UNINSTALL                 = 6;
const ADDON_UPGRADE                   = 7;
const ADDON_DOWNGRADE                 = 8;

// This verifies that bootstrappable add-ons can be used without restarts.
Components.utils.import("resource://gre/modules/Services.jsm");

// Enable loading extensions from the user scopes
Services.prefs.setIntPref("extensions.enabledScopes",
                          AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_USER);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

const profileDir = gProfD.clone();
profileDir.append("extensions");
const userExtDir = gProfD.clone();
userExtDir.append("extensions2");
userExtDir.append(gAppInfo.ID);
registerDirectory("XREUSysExt", userExtDir.parent);

function resetPrefs() {
  Services.prefs.setIntPref("bootstraptest.active_version", -1);
  Services.prefs.setIntPref("bootstraptest.installed_version", -1);
  Services.prefs.setIntPref("bootstraptest.startup_reason", -1);
  Services.prefs.setIntPref("bootstraptest.shutdown_reason", -1);
  Services.prefs.setIntPref("bootstraptest.install_reason", -1);
  Services.prefs.setIntPref("bootstraptest.uninstall_reason", -1);
}

function getActiveVersion() {
  return Services.prefs.getIntPref("bootstraptest.active_version");
}

function getInstalledVersion() {
  return Services.prefs.getIntPref("bootstraptest.installed_version");
}

function getStartupReason() {
  return Services.prefs.getIntPref("bootstraptest.startup_reason");
}

function getShutdownReason() {
  return Services.prefs.getIntPref("bootstraptest.shutdown_reason");
}

function getInstallReason() {
  return Services.prefs.getIntPref("bootstraptest.install_reason");
}

function getUninstallReason() {
  return Services.prefs.getIntPref("bootstraptest.uninstall_reason");
}

function run_test() {
  do_test_pending();

  resetPrefs();

  startupManager();

  run_test_1();
}

// Tests that installing doesn't require a restart
function run_test_1() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_1"), function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.type, "extension");
    do_check_eq(install.version, "1.0");
    do_check_eq(install.name, "Test Bootstrap 1");
    do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
    do_check_true(install.addon.hasResource("install.rdf"));
    do_check_true(install.addon.hasResource("bootstrap.js"));
    do_check_false(install.addon.hasResource("foo.bar"));
    do_check_eq(install.addon.operationsRequiringRestart &
                AddonManager.OP_NEEDS_RESTART_INSTALL, 0);
    do_check_not_in_crash_annotation("bootstrap1@tests.mozilla.org", "1.0");

    prepare_test({
      "bootstrap1@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], check_test_1);
    install.install();
  });
}

function check_test_1() {
  AddonManager.getAllInstalls(function(installs) {
    // There should be no active installs now since the install completed and
    // doesn't require a restart.
    do_check_eq(installs.length, 0);

    AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
      do_check_neq(b1, null);
      do_check_eq(b1.version, "1.0");
      do_check_false(b1.appDisabled);
      do_check_false(b1.userDisabled);
      do_check_true(b1.isActive);
      do_check_eq(getInstalledVersion(), 1);
      do_check_eq(getActiveVersion(), 1);
      do_check_eq(getStartupReason(), ADDON_INSTALL);
      do_check_true(b1.hasResource("install.rdf"));
      do_check_true(b1.hasResource("bootstrap.js"));
      do_check_false(b1.hasResource("foo.bar"));
      do_check_in_crash_annotation("bootstrap1@tests.mozilla.org", "1.0");

      let dir = do_get_addon_root_uri(profileDir, "bootstrap1@tests.mozilla.org");
      do_check_eq(b1.getResourceURI("bootstrap.js").spec, dir + "bootstrap.js");

      AddonManager.getAddonsWithOperationsByTypes(null, function(list) {
        do_check_eq(list.length, 0);

        run_test_2();
      });
    });
  });
}

// Tests that disabling doesn't require a restart
function run_test_2() {
  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    prepare_test({
      "bootstrap1@tests.mozilla.org": [
        ["onDisabling", false],
        "onDisabled"
      ]
    });

    do_check_eq(b1.operationsRequiringRestart &
                AddonManager.OP_NEEDS_RESTART_DISABLE, 0);
    b1.userDisabled = true;
    ensure_test_completed();

    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_false(b1.appDisabled);
    do_check_true(b1.userDisabled);
    do_check_false(b1.isActive);
    do_check_eq(getInstalledVersion(), 1);
    do_check_eq(getActiveVersion(), 0);
    do_check_eq(getShutdownReason(), ADDON_DISABLE);
    do_check_not_in_crash_annotation("bootstrap1@tests.mozilla.org", "1.0");

    AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(newb1) {
      do_check_neq(newb1, null);
      do_check_eq(newb1.version, "1.0");
      do_check_false(newb1.appDisabled);
      do_check_true(newb1.userDisabled);
      do_check_false(newb1.isActive);

      run_test_3();
    });
  });
}

// Test that restarting doesn't accidentally re-enable
function run_test_3() {
  shutdownManager();
  do_check_eq(getInstalledVersion(), 1);
  do_check_eq(getActiveVersion(), 0);
  do_check_eq(getShutdownReason(), ADDON_DISABLE);
  startupManager(false);
  do_check_eq(getInstalledVersion(), 1);
  do_check_eq(getActiveVersion(), 0);
  do_check_eq(getShutdownReason(), ADDON_DISABLE);
  do_check_not_in_crash_annotation("bootstrap1@tests.mozilla.org", "1.0");

  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_false(b1.appDisabled);
    do_check_true(b1.userDisabled);
    do_check_false(b1.isActive);

    run_test_4();
  });
}

// Tests that enabling doesn't require a restart
function run_test_4() {
  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    prepare_test({
      "bootstrap1@tests.mozilla.org": [
        ["onEnabling", false],
        "onEnabled"
      ]
    });

    do_check_eq(b1.operationsRequiringRestart &
                AddonManager.OP_NEEDS_RESTART_ENABLE, 0);
    b1.userDisabled = false;
    ensure_test_completed();

    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_false(b1.appDisabled);
    do_check_false(b1.userDisabled);
    do_check_true(b1.isActive);
    do_check_eq(getInstalledVersion(), 1);
    do_check_eq(getActiveVersion(), 1);
    do_check_eq(getStartupReason(), ADDON_ENABLE);
    do_check_in_crash_annotation("bootstrap1@tests.mozilla.org", "1.0");

    AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(newb1) {
      do_check_neq(newb1, null);
      do_check_eq(newb1.version, "1.0");
      do_check_false(newb1.appDisabled);
      do_check_false(newb1.userDisabled);
      do_check_true(newb1.isActive);

      run_test_5();
    });
  });
}

// Tests that a restart shuts down and restarts the add-on
function run_test_5() {
  shutdownManager();
  do_check_eq(getInstalledVersion(), 1);
  do_check_eq(getActiveVersion(), 0);
  do_check_eq(getShutdownReason(), APP_SHUTDOWN);
  do_check_not_in_crash_annotation("bootstrap1@tests.mozilla.org", "1.0");
  startupManager(false);
  do_check_eq(getInstalledVersion(), 1);
  do_check_eq(getActiveVersion(), 1);
  do_check_eq(getStartupReason(), APP_STARTUP);
  do_check_in_crash_annotation("bootstrap1@tests.mozilla.org", "1.0");

  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_false(b1.appDisabled);
    do_check_false(b1.userDisabled);
    do_check_true(b1.isActive);
    do_check_false(isExtensionInAddonsList(profileDir, b1.id));

    run_test_6();
  });
}

// Tests that installing an upgrade doesn't require a restart
function run_test_6() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_2"), function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.type, "extension");
    do_check_eq(install.version, "2.0");
    do_check_eq(install.name, "Test Bootstrap 1");
    do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);

    prepare_test({
      "bootstrap1@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], check_test_6);
    install.install();
  });
}

function check_test_6() {
  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    do_check_neq(b1, null);
    do_check_eq(b1.version, "2.0");
    do_check_false(b1.appDisabled);
    do_check_false(b1.userDisabled);
    do_check_true(b1.isActive);
    do_check_eq(getInstalledVersion(), 2);
    do_check_eq(getActiveVersion(), 2);
    do_check_eq(getStartupReason(), ADDON_UPGRADE);
    do_check_eq(getShutdownReason(), ADDON_UPGRADE);
    do_check_not_in_crash_annotation("bootstrap1@tests.mozilla.org", "1.0");
    do_check_in_crash_annotation("bootstrap1@tests.mozilla.org", "2.0");

    run_test_7();
  });
}

// Tests that uninstalling doesn't require a restart
function run_test_7() {
  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    prepare_test({
      "bootstrap1@tests.mozilla.org": [
        ["onUninstalling", false],
        "onUninstalled"
      ]
    });

    do_check_eq(b1.operationsRequiringRestart &
                AddonManager.OP_NEEDS_RESTART_UNINSTALL, 0);
    b1.uninstall();

    check_test_7();
  });
}

function check_test_7() {
  ensure_test_completed();
  do_check_eq(getInstalledVersion(), 0);
  do_check_eq(getActiveVersion(), 0);
  do_check_eq(getShutdownReason(), ADDON_UNINSTALL);
  do_check_not_in_crash_annotation("bootstrap1@tests.mozilla.org", "2.0");

  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    do_check_eq(b1, null);

    restartManager();

    AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(newb1) {
      do_check_eq(newb1, null);

      run_test_8();
    });
  });
}

// Test that a bootstrapped extension dropped into the profile loads properly
// on startup and doesn't cause an EM restart
function run_test_8() {
  shutdownManager();

  let dir = profileDir.clone();
  dir.append("bootstrap1@tests.mozilla.org");
  dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0755);
  let zip = AM_Cc["@mozilla.org/libjar/zip-reader;1"].
            createInstance(AM_Ci.nsIZipReader);
  zip.open(do_get_addon("test_bootstrap1_1"));
  dir.append("install.rdf");
  zip.extract("install.rdf", dir);
  dir = dir.parent;
  dir.append("bootstrap.js");
  zip.extract("bootstrap.js", dir);
  zip.close();

  startupManager(false);

  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_false(b1.appDisabled);
    do_check_false(b1.userDisabled);
    do_check_true(b1.isActive);
    do_check_eq(getInstalledVersion(), 1);
    do_check_eq(getActiveVersion(), 1);
    do_check_eq(getStartupReason(), APP_STARTUP);
    do_check_in_crash_annotation("bootstrap1@tests.mozilla.org", "1.0");

    run_test_9();
  });
}

// Test that items detected as removed during startup get removed properly
function run_test_9() {
  shutdownManager();

  let dir = profileDir.clone();
  dir.append("bootstrap1@tests.mozilla.org");
  dir.remove(true);
  startupManager(false);

  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    do_check_eq(b1, null);
    do_check_not_in_crash_annotation("bootstrap1@tests.mozilla.org", "1.0");

    run_test_10();
  });
}


// Tests that installing a downgrade sends the right reason
function run_test_10() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_2"), function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.type, "extension");
    do_check_eq(install.version, "2.0");
    do_check_eq(install.name, "Test Bootstrap 1");
    do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
    do_check_true(install.addon.hasResource("install.rdf"));
    do_check_true(install.addon.hasResource("bootstrap.js"));
    do_check_false(install.addon.hasResource("foo.bar"));
    do_check_not_in_crash_annotation("bootstrap1@tests.mozilla.org", "2.0");

    prepare_test({
      "bootstrap1@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], check_test_10_pt1);
    install.install();
  });
}

function check_test_10_pt1() {
  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    do_check_neq(b1, null);
    do_check_eq(b1.version, "2.0");
    do_check_false(b1.appDisabled);
    do_check_false(b1.userDisabled);
    do_check_true(b1.isActive);
    do_check_eq(getInstalledVersion(), 2);
    do_check_eq(getActiveVersion(), 2);
    do_check_eq(getStartupReason(), ADDON_INSTALL);
    do_check_true(b1.hasResource("install.rdf"));
    do_check_true(b1.hasResource("bootstrap.js"));
    do_check_false(b1.hasResource("foo.bar"));
    do_check_in_crash_annotation("bootstrap1@tests.mozilla.org", "2.0");

    prepare_test({ }, [
      "onNewInstall"
    ]);

    AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_1"), function(install) {
      ensure_test_completed();

      do_check_neq(install, null);
      do_check_eq(install.type, "extension");
      do_check_eq(install.version, "1.0");
      do_check_eq(install.name, "Test Bootstrap 1");
      do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);

      prepare_test({
        "bootstrap1@tests.mozilla.org": [
          ["onInstalling", false],
          "onInstalled"
        ]
      }, [
        "onInstallStarted",
        "onInstallEnded",
      ], check_test_10_pt2);
      install.install();
    });
  });
}

function check_test_10_pt2() {
  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_false(b1.appDisabled);
    do_check_false(b1.userDisabled);
    do_check_true(b1.isActive);
    do_check_eq(getInstalledVersion(), 1);
    do_check_eq(getActiveVersion(), 1);
    do_check_eq(getStartupReason(), ADDON_DOWNGRADE);
    do_check_eq(getShutdownReason(), ADDON_DOWNGRADE);
    do_check_in_crash_annotation("bootstrap1@tests.mozilla.org", "1.0");
    do_check_not_in_crash_annotation("bootstrap1@tests.mozilla.org", "2.0");

    run_test_11();
  });
}

// Tests that uninstalling a disabled add-on still calls the uninstall method
function run_test_11() {
  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    prepare_test({
      "bootstrap1@tests.mozilla.org": [
        ["onDisabling", false],
        "onDisabled",
        ["onUninstalling", false],
        "onUninstalled"
      ]
    });

    b1.userDisabled = true;

    do_check_eq(getInstalledVersion(), 1);
    do_check_eq(getActiveVersion(), 0);
    do_check_eq(getShutdownReason(), ADDON_DISABLE);
    do_check_not_in_crash_annotation("bootstrap1@tests.mozilla.org", "1.0");

    b1.uninstall();

    check_test_11();
  });
}

function check_test_11() {
  ensure_test_completed();
  do_check_eq(getInstalledVersion(), 0);
  do_check_eq(getActiveVersion(), 0);
  do_check_not_in_crash_annotation("bootstrap1@tests.mozilla.org", "1.0");

  run_test_12();
}

// Tests that bootstrapped extensions are correctly loaded even if the app is
// upgraded at the same time
function run_test_12() {
  shutdownManager();

  let dir = profileDir.clone();
  dir.append("bootstrap1@tests.mozilla.org");
  dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0755);
  let zip = AM_Cc["@mozilla.org/libjar/zip-reader;1"].
            createInstance(AM_Ci.nsIZipReader);
  zip.open(do_get_addon("test_bootstrap1_1"));
  dir.append("install.rdf");
  zip.extract("install.rdf", dir);
  dir = dir.parent;
  dir.append("bootstrap.js");
  zip.extract("bootstrap.js", dir);
  zip.close();

  startupManager(true);

  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_false(b1.appDisabled);
    do_check_false(b1.userDisabled);
    do_check_true(b1.isActive);
    do_check_eq(getInstalledVersion(), 1);
    do_check_eq(getActiveVersion(), 1);
    do_check_eq(getStartupReason(), APP_STARTUP);
    do_check_in_crash_annotation("bootstrap1@tests.mozilla.org", "1.0");

    b1.uninstall();
    restartManager();

    run_test_13();
  });
}


// Tests that installing a bootstrapped extension with an invalid application
// entry doesn't call it's startup method
function run_test_13() {
  prepare_test({ }, [
    "onNewInstall"
  ]);

  AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_3"), function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.type, "extension");
    do_check_eq(install.version, "3.0");
    do_check_eq(install.name, "Test Bootstrap 1");
    do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
    do_check_not_in_crash_annotation("bootstrap1@tests.mozilla.org", "3.0");

    prepare_test({
      "bootstrap1@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], check_test_13);
    install.install();
  });
}

function check_test_13() {
  AddonManager.getAllInstalls(function(installs) {
    // There should be no active installs now since the install completed and
    // doesn't require a restart.
    do_check_eq(installs.length, 0);

    AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
      do_check_neq(b1, null);
      do_check_eq(b1.version, "3.0");
      do_check_true(b1.appDisabled);
      do_check_false(b1.userDisabled);
      do_check_false(b1.isActive);
      do_check_eq(getInstalledVersion(), 3);  // We call install even for disabled add-ons
      do_check_eq(getActiveVersion(), 0);     // Should not have called startup though
      do_check_not_in_crash_annotation("bootstrap1@tests.mozilla.org", "3.0");

      restartManager();

      AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
        do_check_neq(b1, null);
        do_check_eq(b1.version, "3.0");
        do_check_true(b1.appDisabled);
        do_check_false(b1.userDisabled);
        do_check_false(b1.isActive);
        do_check_eq(getInstalledVersion(), 3);  // We call install even for disabled add-ons
        do_check_eq(getActiveVersion(), 0);     // Should not have called startup though
        do_check_not_in_crash_annotation("bootstrap1@tests.mozilla.org", "3.0");

        b1.uninstall();
        restartManager();

        run_test_14();
      });
    });
  });
}

// Tests that a bootstrapped extension with an invalid target application entry
// does not get loaded when detected during startup
function run_test_14() {
  shutdownManager();

  let dir = profileDir.clone();
  dir.append("bootstrap1@tests.mozilla.org");
  dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0755);
  let zip = AM_Cc["@mozilla.org/libjar/zip-reader;1"].
            createInstance(AM_Ci.nsIZipReader);
  zip.open(do_get_addon("test_bootstrap1_3"));
  dir.append("install.rdf");
  zip.extract("install.rdf", dir);
  dir = dir.parent;
  dir.append("bootstrap.js");
  zip.extract("bootstrap.js", dir);
  zip.close();

  startupManager(false);

  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    do_check_neq(b1, null);
    do_check_eq(b1.version, "3.0");
    do_check_true(b1.appDisabled);
    do_check_false(b1.userDisabled);
    do_check_false(b1.isActive);
    do_check_eq(getInstalledVersion(), 3);   // We call install even for disabled add-ons
    do_check_eq(getActiveVersion(), 0);      // Should not have called startup though
    do_check_not_in_crash_annotation("bootstrap1@tests.mozilla.org", "3.0");

    b1.uninstall();

    run_test_15();
  });
}

// Tests that upgrading a disabled bootstrapped extension still calls uninstall
// and install but doesn't startup the new version
function run_test_15() {
  installAllFiles([do_get_addon("test_bootstrap1_1")], function() {
    AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
      do_check_neq(b1, null);
      do_check_eq(b1.version, "1.0");
      do_check_false(b1.appDisabled);
      do_check_false(b1.userDisabled);
      do_check_true(b1.isActive);
      do_check_eq(getInstalledVersion(), 1);
      do_check_eq(getActiveVersion(), 1);

      b1.userDisabled = true;
      do_check_false(b1.isActive);
      do_check_eq(getInstalledVersion(), 1);
      do_check_eq(getActiveVersion(), 0);

      prepare_test({ }, [
        "onNewInstall"
      ]);

      AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_2"), function(install) {
        ensure_test_completed();

        do_check_neq(install, null);
        do_check_true(install.addon.userDisabled);

        prepare_test({
          "bootstrap1@tests.mozilla.org": [
            ["onInstalling", false],
            "onInstalled"
          ]
        }, [
          "onInstallStarted",
          "onInstallEnded",
        ], check_test_15);
        install.install();
      });
    });
  });
}

function check_test_15() {
  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    do_check_neq(b1, null);
    do_check_eq(b1.version, "2.0");
    do_check_false(b1.appDisabled);
    do_check_true(b1.userDisabled);
    do_check_false(b1.isActive);
    do_check_eq(getInstalledVersion(), 2);
    do_check_eq(getActiveVersion(), 0);

    restartManager();

    AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
      do_check_neq(b1, null);
      do_check_eq(b1.version, "2.0");
      do_check_false(b1.appDisabled);
      do_check_true(b1.userDisabled);
      do_check_false(b1.isActive);
      do_check_eq(getInstalledVersion(), 2);
      do_check_eq(getActiveVersion(), 0);

      b1.uninstall();

      run_test_16();
    });
  });
}

// Tests that bootstrapped extensions don't get loaded when in safe mode
function run_test_16() {
  installAllFiles([do_get_addon("test_bootstrap1_1")], function() {
    AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
      // Should have installed and started
      do_check_eq(getInstalledVersion(), 1);
      do_check_eq(getActiveVersion(), 1);
      do_check_true(b1.isActive);
      do_check_eq(b1.iconURL, "chrome://foo/skin/icon.png");
      do_check_eq(b1.aboutURL, "chrome://foo/content/about.xul");
      do_check_eq(b1.optionsURL, "chrome://foo/content/options.xul");

      shutdownManager();

      // Should have stopped
      do_check_eq(getInstalledVersion(), 1);
      do_check_eq(getActiveVersion(), 0);

      gAppInfo.inSafeMode = true;
      startupManager(false);

      AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
        // Should still be stopped
        do_check_eq(getInstalledVersion(), 1);
        do_check_eq(getActiveVersion(), 0);
        do_check_false(b1.isActive);
        do_check_eq(b1.iconURL, null);
        do_check_eq(b1.aboutURL, null);
        do_check_eq(b1.optionsURL, null);

        shutdownManager();
        gAppInfo.inSafeMode = false;
        startupManager(false);

        // Should have started
        do_check_eq(getInstalledVersion(), 1);
        do_check_eq(getActiveVersion(), 1);

        AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
          b1.uninstall();

          run_test_17();
        });
      });
    });
  });
}

// Check that a bootstrapped extension in a non-profile location is loaded
function run_test_17() {
  shutdownManager();

  let dir = userExtDir.clone();
  dir.append("bootstrap1@tests.mozilla.org");
  dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0755);
  let zip = AM_Cc["@mozilla.org/libjar/zip-reader;1"].
            createInstance(AM_Ci.nsIZipReader);
  zip.open(do_get_addon("test_bootstrap1_1"));
  dir.append("install.rdf");
  zip.extract("install.rdf", dir);
  dir = dir.parent;
  dir.append("bootstrap.js");
  zip.extract("bootstrap.js", dir);
  zip.close();

  resetPrefs();
  startupManager();

  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    // Should have installed and started
    do_check_eq(getInstalledVersion(), 1);
    do_check_eq(getActiveVersion(), 1);
    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_true(b1.isActive);

    run_test_18();
  });
}

// Check that installing a new bootstrapped extension in the profile replaces
// the existing one
function run_test_18() {
  resetPrefs();
  installAllFiles([do_get_addon("test_bootstrap1_2")], function() {
    AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
      // Should have installed and started
      do_check_eq(getInstalledVersion(), 2);
      do_check_eq(getActiveVersion(), 2);
      do_check_neq(b1, null);
      do_check_eq(b1.version, "2.0");
      do_check_true(b1.isActive);

      do_check_eq(getShutdownReason(), ADDON_UPGRADE);
      do_check_eq(getUninstallReason(), ADDON_UPGRADE);
      do_check_eq(getInstallReason(), ADDON_UPGRADE);
      do_check_eq(getStartupReason(), ADDON_UPGRADE);

      run_test_19();
    });
  });
}

// Check that uninstalling the profile version reveals the non-profile one
function run_test_19() {
  resetPrefs();
  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    // The revealed add-on gets activated asynchronously
    prepare_test({
      "bootstrap1@tests.mozilla.org": [
        ["onUninstalling", false],
        "onUninstalled",
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [], check_test_19);

    b1.uninstall();
  });
}

function check_test_19() {
  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    // Should have reverted to the older version
    do_check_eq(getInstalledVersion(), 1);
    do_check_eq(getActiveVersion(), 1);
    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_true(b1.isActive);

    // TODO these reasons really should be ADDON_DOWNGRADE (bug 607818)
    do_check_eq(getShutdownReason(), ADDON_UNINSTALL);
    do_check_eq(getUninstallReason(), ADDON_UNINSTALL);
    do_check_eq(getInstallReason(), ADDON_INSTALL);
    do_check_eq(getStartupReason(), ADDON_INSTALL);

    run_test_20();
  });
}

// Check that a new profile extension detected at startup replaces the non-profile
// one
function run_test_20() {
  resetPrefs();
  shutdownManager();

  let dir = profileDir.clone();
  dir.append("bootstrap1@tests.mozilla.org");
  dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0755);
  let zip = AM_Cc["@mozilla.org/libjar/zip-reader;1"].
            createInstance(AM_Ci.nsIZipReader);
  zip.open(do_get_addon("test_bootstrap1_2"));
  dir.append("install.rdf");
  zip.extract("install.rdf", dir);
  dir = dir.parent;
  dir.append("bootstrap.js");
  zip.extract("bootstrap.js", dir);
  zip.close();

  startupManager();

  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    // Should have installed and started
    do_check_eq(getInstalledVersion(), 2);
    do_check_eq(getActiveVersion(), 2);
    do_check_neq(b1, null);
    do_check_eq(b1.version, "2.0");
    do_check_true(b1.isActive);

    do_check_eq(getShutdownReason(), APP_SHUTDOWN);
    do_check_eq(getUninstallReason(), ADDON_UPGRADE);
    do_check_eq(getInstallReason(), ADDON_UPGRADE);
    do_check_eq(getStartupReason(), APP_STARTUP);

    run_test_21();
  });
}

// Check that a detected removal reveals the non-profile one
function run_test_21() {
  resetPrefs();
  shutdownManager();

  let dir = profileDir.clone();
  dir.append("bootstrap1@tests.mozilla.org");
  dir.remove(true);

  startupManager();

  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    // Should have installed and started
    do_check_eq(getInstalledVersion(), 1);
    do_check_eq(getActiveVersion(), 1);
    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_true(b1.isActive);

    do_check_eq(getShutdownReason(), APP_SHUTDOWN);

    // This won't be set as the bootstrap script was gone so we couldn't
    // uninstall it properly
    do_check_eq(getUninstallReason(), -1);

    // TODO this reason should probably be ADDON_DOWNGRADE (bug 607818)
    do_check_eq(getInstallReason(), ADDON_INSTALL);

    do_check_eq(getStartupReason(), APP_STARTUP);

    dir = userExtDir.clone();
    dir.append("bootstrap1@tests.mozilla.org");
    dir.remove(true);

    restartManager();

    run_test_22();
  });
}

// Check that an upgrade from the filesystem is detected and applied correctly
function run_test_22() {
  shutdownManager();

  let dir = profileDir.clone();
  dir.append("bootstrap1@tests.mozilla.org");
  dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0755);
  let zip = AM_Cc["@mozilla.org/libjar/zip-reader;1"].
            createInstance(AM_Ci.nsIZipReader);
  zip.open(do_get_addon("test_bootstrap1_1"));
  dir.append("install.rdf");
  zip.extract("install.rdf", dir);
  dir = dir.parent;
  dir.append("bootstrap.js");
  zip.extract("bootstrap.js", dir);
  zip.close();

  // Make it look old so changes are detected
  setExtensionModifiedTime(dir.parent, dir.parent.lastModifiedTime - 5000);

  startupManager();

  AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
    // Should have installed and started
    do_check_eq(getInstalledVersion(), 1);
    do_check_eq(getActiveVersion(), 1);
    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_true(b1.isActive);

    resetPrefs();
    shutdownManager();

    dir = dir.parent;
    dir.remove(true);
    dir.create(AM_Ci.nsIFile.DIRECTORY_TYPE, 0755);
    let zip = AM_Cc["@mozilla.org/libjar/zip-reader;1"].
              createInstance(AM_Ci.nsIZipReader);
    zip.open(do_get_addon("test_bootstrap1_2"));
    dir.append("install.rdf");
    zip.extract("install.rdf", dir);
    dir = dir.parent;
    dir.append("bootstrap.js");
    zip.extract("bootstrap.js", dir);
    zip.close();

    startupManager();

    AddonManager.getAddonByID("bootstrap1@tests.mozilla.org", function(b1) {
      // Should have installed and started
      do_check_eq(getInstalledVersion(), 2);
      do_check_eq(getActiveVersion(), 2);
      do_check_neq(b1, null);
      do_check_eq(b1.version, "2.0");
      do_check_true(b1.isActive);

      do_check_eq(getShutdownReason(), APP_SHUTDOWN);

      // This won't be set as the bootstrap script was gone so we couldn't
      // uninstall it properly
      do_check_eq(getUninstallReason(), -1);

      do_check_eq(getInstallReason(), ADDON_UPGRADE);
      do_check_eq(getStartupReason(), APP_STARTUP);

      do_test_finished();
    });
  });
}
