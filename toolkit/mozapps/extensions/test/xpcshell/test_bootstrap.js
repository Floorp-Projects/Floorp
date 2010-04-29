/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that bootstrappable add-ons can be used with restarts.
Components.utils.import("resource://gre/modules/Services.jsm");

Services.prefs.setIntPref("bootstraptest.version", 0);

const profileDir = gProfD.clone();
profileDir.append("extensions");

function getActivatedVersion() {
  return Services.prefs.getIntPref("bootstraptest.version");
}

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  startupManager(1);

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
    do_check_eq(install.type, "bootstrapped");
    do_check_eq(install.version, "1.0");
    do_check_eq(install.name, "Test Bootstrap 1");
    do_check_eq(install.state, AddonManager.STATE_DOWNLOADED);
    do_check_true(install.addon.hasResource("install.rdf"));
    do_check_true(install.addon.hasResource("bootstrap.js"));
    do_check_false(install.addon.hasResource("foo.bar"));

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
  AddonManager.getAddon("bootstrap1@tests.mozilla.org", function(b1) {
    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_false(b1.appDisabled);
    do_check_false(b1.userDisabled);
    do_check_true(b1.isActive);
    do_check_eq(getActivatedVersion(), 1);
    do_check_true(b1.hasResource("install.rdf"));
    do_check_true(b1.hasResource("bootstrap.js"));
    do_check_false(b1.hasResource("foo.bar"));

    let dir = profileDir.clone();
    dir.append("bootstrap1@tests.mozilla.org");
    dir.append("bootstrap.js");
    let uri = Services.io.newFileURI(dir).spec;
    do_check_eq(b1.getResourceURL("bootstrap.js"), uri);

    AddonManager.getAddonsWithPendingOperations(null, function(list) {
      do_check_eq(list.length, 0);

      run_test_2();
    });
  });
}

// Tests that disabling doesn't require a restart
function run_test_2() {
  AddonManager.getAddon("bootstrap1@tests.mozilla.org", function(b1) {
    prepare_test({
      "bootstrap1@tests.mozilla.org": [
        ["onDisabling", false],
        "onDisabled"
      ]
    });

    b1.userDisabled = true;
    ensure_test_completed();

    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_false(b1.appDisabled);
    do_check_true(b1.userDisabled);
    do_check_false(b1.isActive);
    do_check_eq(getActivatedVersion(), 0);

    AddonManager.getAddon("bootstrap1@tests.mozilla.org", function(newb1) {
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
  do_check_eq(getActivatedVersion(), 0);
  startupManager(0, false);
  do_check_eq(getActivatedVersion(), 0);

  AddonManager.getAddon("bootstrap1@tests.mozilla.org", function(b1) {
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
  AddonManager.getAddon("bootstrap1@tests.mozilla.org", function(b1) {
    prepare_test({
      "bootstrap1@tests.mozilla.org": [
        ["onEnabling", false],
        "onEnabled"
      ]
    });

    b1.userDisabled = false;
    ensure_test_completed();

    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_false(b1.appDisabled);
    do_check_false(b1.userDisabled);
    do_check_true(b1.isActive);
    do_check_eq(getActivatedVersion(), 1);

    AddonManager.getAddon("bootstrap1@tests.mozilla.org", function(newb1) {
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
  do_check_eq(getActivatedVersion(), 0);
  startupManager(0, false);
  do_check_eq(getActivatedVersion(), 1);

  AddonManager.getAddon("bootstrap1@tests.mozilla.org", function(b1) {
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
    do_check_eq(install.type, "bootstrapped");
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
  AddonManager.getAddon("bootstrap1@tests.mozilla.org", function(b1) {
    do_check_neq(b1, null);
    do_check_eq(b1.version, "2.0");
    do_check_false(b1.appDisabled);
    do_check_false(b1.userDisabled);
    do_check_true(b1.isActive);
    do_check_eq(getActivatedVersion(), 2);

    run_test_7();
  });
}

// Tests that uninstalling doesn't require a restart
function run_test_7() {
  AddonManager.getAddon("bootstrap1@tests.mozilla.org", function(b1) {
    prepare_test({
      "bootstrap1@tests.mozilla.org": [
        ["onUninstalling", false],
        "onUninstalled"
      ]
    });

    b1.uninstall();

    check_test_7();
  });
}

function check_test_7() {
  ensure_test_completed();
  do_check_eq(getActivatedVersion(), 0);

  AddonManager.getAddon("bootstrap1@tests.mozilla.org", function(b1) {
    do_check_eq(b1, null);

    restartManager(0);

    AddonManager.getAddon("bootstrap1@tests.mozilla.org", function(newb1) {
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

  startupManager(0, false);

  AddonManager.getAddon("bootstrap1@tests.mozilla.org", function(b1) {
    do_check_neq(b1, null);
    do_check_eq(b1.version, "1.0");
    do_check_false(b1.appDisabled);
    do_check_false(b1.userDisabled);
    do_check_true(b1.isActive);
    do_check_eq(getActivatedVersion(), 1);

    run_test_9();
  });
}

// Test that items detected as removed during startup don't force an EM restart
function run_test_9() {
  shutdownManager();

  let dir = profileDir.clone();
  dir.append("bootstrap1@tests.mozilla.org");
  dir.remove(true);
  startupManager(0, false);

  AddonManager.getAddon("bootstrap1@tests.mozilla.org", function(b1) {
    do_check_eq(b1, null);

    do_test_finished();
  });
}
