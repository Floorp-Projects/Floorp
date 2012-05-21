/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const URI_EXTENSION_UPDATE_DIALOG     = "chrome://mozapps/content/extensions/update.xul";
const PREF_EM_SHOW_MISMATCH_UI        = "extensions.showMismatchUI";

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

do_load_httpd_js();
var testserver;

const profileDir = gProfD.clone();
profileDir.append("extensions");

var gInstallUpdate = false;
var gCheckUpdates = false;

// This will be called to show the compatibility update dialog.
var WindowWatcher = {
  expected: false,
  arguments: null,

  openWindow: function(parent, url, name, features, arguments) {
    do_check_true(Services.startup.interrupted);
    do_check_eq(url, URI_EXTENSION_UPDATE_DIALOG);
    do_check_true(this.expected);
    this.expected = false;
    this.arguments = arguments.QueryInterface(AM_Ci.nsIVariant);

    var updated = !gCheckUpdates;
    if (gCheckUpdates) {
      AddonManager.getAddonByID("bug542391_6@tests.mozilla.org", function(a6) {
        a6.findUpdates({
          onUpdateFinished: function() {
            AddonManagerPrivate.removeStartupChange("disabled", "bug542391_6@tests.mozilla.org");
            updated = true;
          }
        }, AddonManager.UPDATE_WHEN_NEW_APP_INSTALLED);
      });
    }

    var installed = !gInstallUpdate;
    if (gInstallUpdate) {
      // Simulate installing an update while in the dialog
      installAllFiles([do_get_addon("test_bug542391_3_2")], function() {
        AddonManagerPrivate.removeStartupChange("disabled", "bug542391_3@tests.mozilla.org");
        AddonManagerPrivate.addStartupChange("updated", "bug542391_3@tests.mozilla.org");
        installed = true;
      });
    }

    // The dialog is meant to be opened modally and the install operation can be
    // asynchronous, so we must spin an event loop (like the modal window does)
    // until the install is complete
    let thr = AM_Cc["@mozilla.org/thread-manager;1"].
              getService(AM_Ci.nsIThreadManager).
              mainThread;

    while (!installed || !updated)
      thr.processNextEvent(false);
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIWindowWatcher)
     || iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  }
}

var WindowWatcherFactory = {
  createInstance: function createInstance(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return WindowWatcher.QueryInterface(iid);
  }
};

var registrar = Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{1dfeb90a-2193-45d5-9cb8-864928b2af55}"),
                          "Fake Window Watcher",
                          "@mozilla.org/embedcomp/window-watcher;1", WindowWatcherFactory);

function check_state_v1([a1, a2, a3, a4, a5, a6]) {
  do_check_neq(a1, null);
  do_check_false(a1.appDisabled);
  do_check_false(a1.userDisabled);
  do_check_true(a1.isActive);
  do_check_true(isExtensionInAddonsList(profileDir, a1.id));

  do_check_neq(a2, null);
  do_check_false(a2.appDisabled);
  do_check_true(a2.userDisabled);
  do_check_false(a2.isActive);
  do_check_false(isExtensionInAddonsList(profileDir, a2.id));

  do_check_neq(a3, null);
  do_check_false(a3.appDisabled);
  do_check_false(a3.userDisabled);
  do_check_true(a3.isActive);
  do_check_true(isExtensionInAddonsList(profileDir, a3.id));
  do_check_eq(a3.version, "1.0");

  do_check_neq(a4, null);
  do_check_false(a4.appDisabled);
  do_check_true(a4.userDisabled);
  do_check_false(a4.isActive);
  do_check_false(isExtensionInAddonsList(profileDir, a4.id));

  do_check_neq(a5, null);
  do_check_false(a5.appDisabled);
  do_check_false(a5.userDisabled);
  do_check_true(a5.isActive);
  do_check_true(isExtensionInAddonsList(profileDir, a5.id));

  do_check_neq(a6, null);
  do_check_false(a6.appDisabled);
  do_check_false(a6.userDisabled);
  do_check_true(a6.isActive);
  do_check_true(isExtensionInAddonsList(profileDir, a6.id));
}

function check_state_v1_2([a1, a2, a3, a4, a5, a6]) {
  do_check_neq(a1, null);
  do_check_false(a1.appDisabled);
  do_check_false(a1.userDisabled);
  do_check_true(a1.isActive);
  do_check_true(isExtensionInAddonsList(profileDir, a1.id));

  do_check_neq(a2, null);
  do_check_false(a2.appDisabled);
  do_check_true(a2.userDisabled);
  do_check_false(a2.isActive);
  do_check_false(isExtensionInAddonsList(profileDir, a2.id));

  do_check_neq(a3, null);
  do_check_true(a3.appDisabled);
  do_check_false(a3.userDisabled);
  do_check_false(a3.isActive);
  do_check_false(isExtensionInAddonsList(profileDir, a3.id));
  do_check_eq(a3.version, "2.0");

  do_check_neq(a4, null);
  do_check_false(a4.appDisabled);
  do_check_true(a4.userDisabled);
  do_check_false(a4.isActive);
  do_check_false(isExtensionInAddonsList(profileDir, a4.id));

  do_check_neq(a5, null);
  do_check_false(a5.appDisabled);
  do_check_false(a5.userDisabled);
  do_check_true(a5.isActive);
  do_check_true(isExtensionInAddonsList(profileDir, a5.id));

  do_check_neq(a6, null);
  do_check_false(a6.appDisabled);
  do_check_false(a6.userDisabled);
  do_check_true(a6.isActive);
  do_check_true(isExtensionInAddonsList(profileDir, a6.id));
}

function check_state_v2([a1, a2, a3, a4, a5, a6]) {
  do_check_neq(a1, null);
  do_check_true(a1.appDisabled);
  do_check_false(a1.userDisabled);
  do_check_false(a1.isActive);
  do_check_false(isExtensionInAddonsList(profileDir, a1.id));

  do_check_neq(a2, null);
  do_check_false(a2.appDisabled);
  do_check_true(a2.userDisabled);
  do_check_false(a2.isActive);
  do_check_false(isExtensionInAddonsList(profileDir, a2.id));

  do_check_neq(a3, null);
  do_check_false(a3.appDisabled);
  do_check_false(a3.userDisabled);
  do_check_true(a3.isActive);
  do_check_true(isExtensionInAddonsList(profileDir, a3.id));
  do_check_eq(a3.version, "1.0");

  do_check_neq(a4, null);
  do_check_false(a4.appDisabled);
  do_check_true(a4.userDisabled);
  do_check_false(a4.isActive);
  do_check_false(isExtensionInAddonsList(profileDir, a4.id));

  do_check_neq(a5, null);
  do_check_false(a5.appDisabled);
  do_check_false(a5.userDisabled);
  do_check_true(a5.isActive);
  do_check_true(isExtensionInAddonsList(profileDir, a5.id));

  do_check_neq(a6, null);
  do_check_false(a6.appDisabled);
  do_check_false(a6.userDisabled);
  do_check_true(a6.isActive);
  do_check_true(isExtensionInAddonsList(profileDir, a6.id));
}

function check_state_v3([a1, a2, a3, a4, a5, a6]) {
  do_check_neq(a1, null);
  do_check_true(a1.appDisabled);
  do_check_false(a1.userDisabled);
  do_check_false(a1.isActive);
  do_check_false(isExtensionInAddonsList(profileDir, a1.id));

  do_check_neq(a2, null);
  do_check_true(a2.appDisabled);
  do_check_true(a2.userDisabled);
  do_check_false(a2.isActive);
  do_check_false(isExtensionInAddonsList(profileDir, a2.id));

  do_check_neq(a3, null);
  do_check_true(a3.appDisabled);
  do_check_false(a3.userDisabled);
  do_check_false(a3.isActive);
  do_check_false(isExtensionInAddonsList(profileDir, a3.id));
  do_check_eq(a3.version, "1.0");

  do_check_neq(a4, null);
  do_check_false(a4.appDisabled);
  do_check_true(a4.userDisabled);
  do_check_false(a4.isActive);
  do_check_false(isExtensionInAddonsList(profileDir, a4.id));

  do_check_neq(a5, null);
  do_check_false(a5.appDisabled);
  do_check_false(a5.userDisabled);
  do_check_true(a5.isActive);
  do_check_true(isExtensionInAddonsList(profileDir, a5.id));

  do_check_neq(a6, null);
  do_check_false(a6.appDisabled);
  do_check_false(a6.userDisabled);
  do_check_true(a6.isActive);
  do_check_true(isExtensionInAddonsList(profileDir, a6.id));
}

function check_state_v3_2([a1, a2, a3, a4, a5, a6]) {
  do_check_neq(a1, null);
  do_check_true(a1.appDisabled);
  do_check_false(a1.userDisabled);
  do_check_false(a1.isActive);
  do_check_false(isExtensionInAddonsList(profileDir, a1.id));

  do_check_neq(a2, null);
  do_check_true(a2.appDisabled);
  do_check_true(a2.userDisabled);
  do_check_false(a2.isActive);
  do_check_false(isExtensionInAddonsList(profileDir, a2.id));

  do_check_neq(a3, null);
  do_check_false(a3.appDisabled);
  do_check_false(a3.userDisabled);
  do_check_true(a3.isActive);
  do_check_true(isExtensionInAddonsList(profileDir, a3.id));
  do_check_eq(a3.version, "2.0");

  do_check_neq(a4, null);
  do_check_false(a4.appDisabled);
  do_check_true(a4.userDisabled);
  do_check_false(a4.isActive);
  do_check_false(isExtensionInAddonsList(profileDir, a4.id));

  do_check_neq(a5, null);
  do_check_false(a5.appDisabled);
  do_check_false(a5.userDisabled);
  do_check_true(a5.isActive);
  do_check_true(isExtensionInAddonsList(profileDir, a5.id));

  do_check_neq(a6, null);
  do_check_false(a6.appDisabled);
  do_check_false(a6.userDisabled);
  do_check_true(a6.isActive);
  do_check_true(isExtensionInAddonsList(profileDir, a6.id));
}

// Install all the test add-ons, disable two of them and "upgrade" the app to
// version 2 which will appDisable one.
function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  Services.prefs.setBoolPref(PREF_EM_SHOW_MISMATCH_UI, true);

  // Add an extension to the profile to make sure the dialog doesn't show up
  // on new profiles
  var dest = writeInstallRDFForExtension({
    id: "addon1@tests.mozilla.org",
    version: "1.0",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }],
    name: "Test Addon 1",
  }, profileDir);

  // Create and configure the HTTP server.
  testserver = new nsHttpServer();
  testserver.registerDirectory("/data/", do_get_file("data"));
  testserver.registerDirectory("/addons/", do_get_file("addons"));
  testserver.start(4444);

  startupManager();

  dest.remove(true);

  installAllFiles([do_get_addon("test_bug542391_1"),
                   do_get_addon("test_bug542391_2"),
                   do_get_addon("test_bug542391_3_1"),
                   do_get_addon("test_bug542391_4"),
                   do_get_addon("test_bug542391_5"),
                   do_get_addon("test_bug542391_6")], function() {

    restartManager();
    check_startup_changes("installed", []);
    check_startup_changes("updated", []);
    check_startup_changes("uninstalled", ["addon1@tests.mozilla.org"]);
    check_startup_changes("disabled", []);
    check_startup_changes("enabled", []);

    AddonManager.getAddonsByIDs(["bug542391_2@tests.mozilla.org",
                                 "bug542391_4@tests.mozilla.org"],
                                 function([a2, a4]) {
      a2.userDisabled = true;
      a4.userDisabled = true;
      restartManager();
      check_startup_changes("installed", []);
      check_startup_changes("updated", []);
      check_startup_changes("uninstalled", []);
      check_startup_changes("disabled", []);
      check_startup_changes("enabled", []);

      AddonManager.getAddonsByIDs(["bug542391_1@tests.mozilla.org",
                                   "bug542391_2@tests.mozilla.org",
                                   "bug542391_3@tests.mozilla.org",
                                   "bug542391_4@tests.mozilla.org",
                                   "bug542391_5@tests.mozilla.org",
                                   "bug542391_6@tests.mozilla.org"],
                                   function(addons) {
        check_state_v1(addons);

        WindowWatcher.expected = true;
        restartManager("2");
        check_startup_changes("installed", []);
        check_startup_changes("updated", []);
        check_startup_changes("uninstalled", []);
        check_startup_changes("disabled", ["bug542391_1@tests.mozilla.org"]);
        check_startup_changes("enabled", []);
        do_check_false(WindowWatcher.expected);

        AddonManager.getAddonsByIDs(["bug542391_1@tests.mozilla.org",
                                     "bug542391_2@tests.mozilla.org",
                                     "bug542391_3@tests.mozilla.org",
                                     "bug542391_4@tests.mozilla.org",
                                     "bug542391_5@tests.mozilla.org",
                                     "bug542391_6@tests.mozilla.org"],
                                     function(addons) {
          check_state_v2(addons);

          run_test_1();
        });
      });
    });
  });
}

function end_test() {
  testserver.stop(do_test_finished);
}

// Upgrade to version 3 which will appDisable two more add-ons. Check that the
// 3 already disabled add-ons were passed to the mismatch dialog.
function run_test_1() {
  gCheckUpdates = true;
  WindowWatcher.expected = true;
  restartManager("3");
  check_startup_changes("installed", []);
  check_startup_changes("updated", []);
  check_startup_changes("uninstalled", []);
  check_startup_changes("disabled", ["bug542391_3@tests.mozilla.org"]);
  check_startup_changes("enabled", []);
  do_check_false(WindowWatcher.expected);
  gCheckUpdates = false;

  AddonManager.getAddonsByIDs(["bug542391_1@tests.mozilla.org",
                               "bug542391_2@tests.mozilla.org",
                               "bug542391_3@tests.mozilla.org",
                               "bug542391_4@tests.mozilla.org",
                               "bug542391_5@tests.mozilla.org",
                               "bug542391_6@tests.mozilla.org"],
                               function(addons) {
    check_state_v3(addons);

    do_check_eq(WindowWatcher.arguments.length, 3);
    do_check_true(WindowWatcher.arguments.indexOf("bug542391_1@tests.mozilla.org") >= 0);
    do_check_true(WindowWatcher.arguments.indexOf("bug542391_2@tests.mozilla.org") >= 0);
    do_check_true(WindowWatcher.arguments.indexOf("bug542391_4@tests.mozilla.org") >= 0);

    run_test_2();
  });
}

// Downgrade to version 2 which will remove appDisable from two add-ons and
// should pass all 4 previously disabled add-ons.
function run_test_2() {
  WindowWatcher.expected = true;
  restartManager("2");
  check_startup_changes("installed", []);
  check_startup_changes("updated", []);
  check_startup_changes("uninstalled", []);
  check_startup_changes("disabled", []);
  check_startup_changes("enabled", ["bug542391_3@tests.mozilla.org"]);
  do_check_false(WindowWatcher.expected);

  AddonManager.getAddonsByIDs(["bug542391_1@tests.mozilla.org",
                               "bug542391_2@tests.mozilla.org",
                               "bug542391_3@tests.mozilla.org",
                               "bug542391_4@tests.mozilla.org",
                               "bug542391_5@tests.mozilla.org",
                               "bug542391_6@tests.mozilla.org"],
                               function(addons) {
    check_state_v2(addons);

    do_check_eq(WindowWatcher.arguments.length, 4);
    do_check_true(WindowWatcher.arguments.indexOf("bug542391_1@tests.mozilla.org") >= 0);
    do_check_true(WindowWatcher.arguments.indexOf("bug542391_2@tests.mozilla.org") >= 0);
    do_check_true(WindowWatcher.arguments.indexOf("bug542391_3@tests.mozilla.org") >= 0);
    do_check_true(WindowWatcher.arguments.indexOf("bug542391_4@tests.mozilla.org") >= 0);

    run_test_5();
  });
}

// Upgrade to version 3 which will appDisable two more add-ons. Check that when
// the upgrade dialog updates an add-on no restart is necessary
function run_test_5() {
  Services.prefs.setBoolPref(PREF_EM_SHOW_MISMATCH_UI, true);
  gInstallUpdate = true;

  WindowWatcher.expected = true;
  restartManager("3");
  check_startup_changes("installed", []);
  check_startup_changes("updated", ["bug542391_3@tests.mozilla.org"]);
  check_startup_changes("uninstalled", []);
  check_startup_changes("disabled", []);
  check_startup_changes("enabled", []);
  do_check_false(WindowWatcher.expected);
  gInstallUpdate = false;

  AddonManager.getAddonsByIDs(["bug542391_1@tests.mozilla.org",
                               "bug542391_2@tests.mozilla.org",
                               "bug542391_3@tests.mozilla.org",
                               "bug542391_4@tests.mozilla.org",
                               "bug542391_5@tests.mozilla.org",
                               "bug542391_6@tests.mozilla.org"],
                               function(addons) {
    check_state_v3_2(addons);

    do_check_eq(WindowWatcher.arguments.length, 3);
    do_check_true(WindowWatcher.arguments.indexOf("bug542391_1@tests.mozilla.org") >= 0);
    do_check_true(WindowWatcher.arguments.indexOf("bug542391_2@tests.mozilla.org") >= 0);
    do_check_true(WindowWatcher.arguments.indexOf("bug542391_4@tests.mozilla.org") >= 0);

    run_test_6();
  });
}

// Downgrade to version 1 which will appEnable all the add-ons
function run_test_6() {
  WindowWatcher.expected = true;
  restartManager("1");
  check_startup_changes("installed", []);
  check_startup_changes("updated", []);
  check_startup_changes("uninstalled", []);
  check_startup_changes("disabled", ["bug542391_3@tests.mozilla.org"]);
  check_startup_changes("enabled", ["bug542391_1@tests.mozilla.org"]);
  do_check_false(WindowWatcher.expected);

  AddonManager.getAddonsByIDs(["bug542391_1@tests.mozilla.org",
                               "bug542391_2@tests.mozilla.org",
                               "bug542391_3@tests.mozilla.org",
                               "bug542391_4@tests.mozilla.org",
                               "bug542391_5@tests.mozilla.org",
                               "bug542391_6@tests.mozilla.org"],
                               function(addons) {
    check_state_v1_2(addons);

    finish_test();
  });
}

function finish_test() {
  do_test_finished();
}
