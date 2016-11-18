/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that language packs can be used without restarts.
Components.utils.import("resource://gre/modules/Services.jsm");

// Enable loading extensions from the user scopes
Services.prefs.setIntPref("extensions.enabledScopes",
                          AddonManager.SCOPE_PROFILE + AddonManager.SCOPE_USER);
// Enable installing distribution add-ons
Services.prefs.setBoolPref("extensions.installDistroAddons", true);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

const profileDir = gProfD.clone();
profileDir.append("extensions");
const userExtDir = gProfD.clone();
userExtDir.append("extensions2");
userExtDir.append(gAppInfo.ID);
registerDirectory("XREUSysExt", userExtDir.parent);
const distroDir = gProfD.clone();
distroDir.append("distribution");
distroDir.append("extensions");
registerDirectory("XREAppDist", distroDir.parent);

var chrome = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
  .getService(Components.interfaces.nsIXULChromeRegistry);

function do_unregister_manifest() {
  let path = getFileForAddon(profileDir, "langpack-x-testing@tests.mozilla.org");
  Components.manager.removeBootstrappedManifestLocation(path);
}

function do_check_locale_not_registered(provider) {
  let didThrow = false;
  try {
    chrome.getSelectedLocale(provider);
  } catch (e) {
    didThrow = true;
  }
  do_check_true(didThrow);
}

function run_test() {
  do_test_pending();

  startupManager();

  run_test_1();
}

// Tests that installing doesn't require a restart
function run_test_1() {
  do_check_locale_not_registered("test-langpack");

  prepare_test({ }, [
    "onNewInstall"
  ]);

  AddonManager.getInstallForFile(do_get_addon("test_langpack"), function(install) {
    ensure_test_completed();

    do_check_neq(install, null);
    do_check_eq(install.type, "locale");
    do_check_eq(install.version, "1.0");
    do_check_eq(install.name, "Language Pack x-testing");
    do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
    do_check_true(install.addon.hasResource("install.rdf"));
    do_check_false(install.addon.hasResource("bootstrap.js"));
    do_check_eq(install.addon.operationsRequiringRestart &
                AddonManager.OP_NEEDS_RESTART_INSTALL, 0);

    let addon = install.addon;
    prepare_test({
      "langpack-x-testing@tests.mozilla.org": [
        ["onInstalling", false],
        "onInstalled"
      ]
    }, [
      "onInstallStarted",
      "onInstallEnded",
    ], function() {
      do_check_true(addon.hasResource("install.rdf"));
      // spin to let the startup complete
      do_execute_soon(check_test_1);
    });
    install.install();
  });
}

function check_test_1() {
  AddonManager.getAllInstalls(function(installs) {
    // There should be no active installs now since the install completed and
    // doesn't require a restart.
    do_check_eq(installs.length, 0);

    AddonManager.getAddonByID("langpack-x-testing@tests.mozilla.org", function(b1) {
      do_check_neq(b1, null);
      do_check_eq(b1.version, "1.0");
      do_check_false(b1.appDisabled);
      do_check_false(b1.userDisabled);
      do_check_true(b1.isActive);
      // check chrome reg that language pack is registered
      do_check_eq(chrome.getSelectedLocale("test-langpack"), "x-testing");
      do_check_true(b1.hasResource("install.rdf"));
      do_check_false(b1.hasResource("bootstrap.js"));

      AddonManager.getAddonsWithOperationsByTypes(null, function(list) {
        do_check_eq(list.length, 0);

        run_test_2();
      });
    });
  });
}

// Tests that disabling doesn't require a restart
function run_test_2() {
  AddonManager.getAddonByID("langpack-x-testing@tests.mozilla.org", function(b1) {
    prepare_test({
      "langpack-x-testing@tests.mozilla.org": [
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
    // check chrome reg that language pack is not registered
    do_check_locale_not_registered("test-langpack");

    AddonManager.getAddonByID("langpack-x-testing@tests.mozilla.org", function(newb1) {
      do_check_neq(newb1, null);
      do_check_eq(newb1.version, "1.0");
      do_check_false(newb1.appDisabled);
      do_check_true(newb1.userDisabled);
      do_check_false(newb1.isActive);

      do_execute_soon(run_test_3);
    });
  });
}

// Test that restarting doesn't accidentally re-enable
function run_test_3() {
  shutdownManager();
  startupManager(false);
  // check chrome reg that language pack is not registered
  do_check_locale_not_registered("test-langpack");

  AddonManager.getAddonByID("langpack-x-testing@tests.mozilla.org", function(b1) {
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
  AddonManager.getAddonByID("langpack-x-testing@tests.mozilla.org", function(b1) {
    prepare_test({
      "langpack-x-testing@tests.mozilla.org": [
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
    // check chrome reg that language pack is registered
    do_check_eq(chrome.getSelectedLocale("test-langpack"), "x-testing");

    AddonManager.getAddonByID("langpack-x-testing@tests.mozilla.org", function(newb1) {
      do_check_neq(newb1, null);
      do_check_eq(newb1.version, "1.0");
      do_check_false(newb1.appDisabled);
      do_check_false(newb1.userDisabled);
      do_check_true(newb1.isActive);

      do_execute_soon(run_test_5);
    });
  });
}

// Tests that a restart shuts down and restarts the add-on
function run_test_5() {
  shutdownManager();
  do_unregister_manifest();
  // check chrome reg that language pack is not registered
  do_check_locale_not_registered("test-langpack");
  startupManager(false);
  // check chrome reg that language pack is registered
  do_check_eq(chrome.getSelectedLocale("test-langpack"), "x-testing");

  AddonManager.getAddonByID("langpack-x-testing@tests.mozilla.org", function(b1) {
    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_false(b1.appDisabled);
    do_check_false(b1.userDisabled);
    do_check_true(b1.isActive);
    do_check_false(isExtensionInAddonsList(profileDir, b1.id));

    run_test_7();
  });
}

// Tests that uninstalling doesn't require a restart
function run_test_7() {
  AddonManager.getAddonByID("langpack-x-testing@tests.mozilla.org", function(b1) {
    prepare_test({
      "langpack-x-testing@tests.mozilla.org": [
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
  // check chrome reg that language pack is not registered
  do_check_locale_not_registered("test-langpack");

  AddonManager.getAddonByID("langpack-x-testing@tests.mozilla.org",
   callback_soon(function(b1) {
    do_check_eq(b1, null);

    restartManager();

    AddonManager.getAddonByID("langpack-x-testing@tests.mozilla.org", function(newb1) {
      do_check_eq(newb1, null);

      do_execute_soon(run_test_8);
    });
  }));
}

// Tests that a locale detected in the profile starts working immediately
function run_test_8() {
  shutdownManager();

  manuallyInstall(do_get_addon("test_langpack"), profileDir, "langpack-x-testing@tests.mozilla.org");

  startupManager(false);

  AddonManager.getAddonByID("langpack-x-testing@tests.mozilla.org",
   callback_soon(function(b1) {
    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_false(b1.appDisabled);
    do_check_false(b1.userDisabled);
    do_check_true(b1.isActive);
    // check chrome reg that language pack is registered
    do_check_eq(chrome.getSelectedLocale("test-langpack"), "x-testing");
    do_check_true(b1.hasResource("install.rdf"));
    do_check_false(b1.hasResource("bootstrap.js"));

    shutdownManager();
    do_unregister_manifest();
    // check chrome reg that language pack is not registered
    do_check_locale_not_registered("test-langpack");
    startupManager(false);
    // check chrome reg that language pack is registered
    do_check_eq(chrome.getSelectedLocale("test-langpack"), "x-testing");

    AddonManager.getAddonByID("langpack-x-testing@tests.mozilla.org", function(b2) {
      prepare_test({
        "langpack-x-testing@tests.mozilla.org": [
          ["onUninstalling", false],
          "onUninstalled"
        ]
      });

      b2.uninstall();
      ensure_test_completed();
      do_execute_soon(run_test_9);
    });
  }));
}

// Tests that a locale from distribution/extensions gets installed and starts
// working immediately
function run_test_9() {
  shutdownManager();
  manuallyInstall(do_get_addon("test_langpack"), distroDir, "langpack-x-testing@tests.mozilla.org");
  gAppInfo.version = "2.0";
  startupManager(true);

  AddonManager.getAddonByID("langpack-x-testing@tests.mozilla.org", callback_soon(function(b1) {
    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_false(b1.appDisabled);
    do_check_false(b1.userDisabled);
    do_check_true(b1.isActive);
    // check chrome reg that language pack is registered
    do_check_eq(chrome.getSelectedLocale("test-langpack"), "x-testing");
    do_check_true(b1.hasResource("install.rdf"));
    do_check_false(b1.hasResource("bootstrap.js"));

    shutdownManager();
    do_unregister_manifest();
    // check chrome reg that language pack is not registered
    do_check_locale_not_registered("test-langpack");
    startupManager(false);
    // check chrome reg that language pack is registered
    do_check_eq(chrome.getSelectedLocale("test-langpack"), "x-testing");

    do_test_finished();
  }));
}
