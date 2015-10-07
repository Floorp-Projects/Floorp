/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const URI_EXTENSION_UPDATE_DIALOG     = "chrome://mozapps/content/extensions/update.xul";
const PREF_EM_SHOW_MISMATCH_UI        = "extensions.showMismatchUI";

// The test extension uses an insecure update url.
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://testing-common/MockRegistrar.jsm");
var testserver;

const profileDir = gProfD.clone();
profileDir.append("extensions");

var gInstallUpdate = false;
var gCheckUpdates = false;

// This will be called to show the compatibility update dialog.
var WindowWatcher = {
  expected: false,
  args: null,

  openWindow: function(parent, url, name, features, args) {
    do_check_true(Services.startup.interrupted);
    do_check_eq(url, URI_EXTENSION_UPDATE_DIALOG);
    do_check_true(this.expected);
    this.expected = false;
    this.args = args.QueryInterface(AM_Ci.nsIVariant);

    var updated = !gCheckUpdates;
    if (gCheckUpdates) {
      AddonManager.getAddonByID("override1x2-1x3@tests.mozilla.org", function(a6) {
        a6.findUpdates({
          onUpdateFinished: function() {
            AddonManagerPrivate.removeStartupChange("disabled", "override1x2-1x3@tests.mozilla.org");
            updated = true;
          }
        }, AddonManager.UPDATE_WHEN_NEW_APP_INSTALLED);
      });
    }

    var installed = !gInstallUpdate;
    if (gInstallUpdate) {
      // Simulate installing an update while in the dialog
      installAllFiles([do_get_addon("upgradeable1x2-3_2")], function() {
        AddonManagerPrivate.removeStartupChange("disabled", "upgradeable1x2-3@tests.mozilla.org");
        AddonManagerPrivate.addStartupChange("updated", "upgradeable1x2-3@tests.mozilla.org");
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

MockRegistrar.register("@mozilla.org/embedcomp/window-watcher;1", WindowWatcher);

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
add_task(function* init() {
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
  testserver = new HttpServer();
  testserver.registerDirectory("/data/", do_get_file("data"));
  testserver.registerDirectory("/addons/", do_get_file("addons"));
  testserver.start(4444);

  startupManager();

  // Remove the add-on we installed directly in the profile directory;
  // this should show as uninstalled on next restart
  dest.remove(true);

  // Load up an initial set of add-ons
  yield promiseInstallAllFiles([do_get_addon("min1max1"),
                                do_get_addon("min1max2"),
                                do_get_addon("upgradeable1x2-3_1"),
                                do_get_addon("min1max3"),
                                do_get_addon("min1max3b"),
                                do_get_addon("override1x2-1x3")]);
  yield promiseRestartManager();

  check_startup_changes("installed", []);
  check_startup_changes("updated", []);
  check_startup_changes("uninstalled", ["addon1@tests.mozilla.org"]);
  check_startup_changes("disabled", []);
  check_startup_changes("enabled", []);

  // user-disable two add-ons
  let [a2, a4] = yield promiseAddonsByIDs(["min1max2@tests.mozilla.org",
                                           "min1max3@tests.mozilla.org"]);
  do_check_true(a2 != null && a4 != null);
  a2.userDisabled = true;
  a4.userDisabled = true;
  yield promiseRestartManager();
  check_startup_changes("installed", []);
  check_startup_changes("updated", []);
  check_startup_changes("uninstalled", []);
  check_startup_changes("disabled", []);
  check_startup_changes("enabled", []);

  let addons = yield promiseAddonsByIDs(["min1max1@tests.mozilla.org",
                                         "min1max2@tests.mozilla.org",
                                         "upgradeable1x2-3@tests.mozilla.org",
                                         "min1max3@tests.mozilla.org",
                                         "min1max3b@tests.mozilla.org",
                                         "override1x2-1x3@tests.mozilla.org"]);
  check_state_v1(addons);

  // Restart as version 2, add-on _1 should become app-disabled
  WindowWatcher.expected = true;
  yield promiseRestartManager("2");
  check_startup_changes("installed", []);
  check_startup_changes("updated", []);
  check_startup_changes("uninstalled", []);
  check_startup_changes("disabled", ["min1max1@tests.mozilla.org"]);
  check_startup_changes("enabled", []);
  do_check_false(WindowWatcher.expected);

  addons = yield promiseAddonsByIDs(["min1max1@tests.mozilla.org",
                                     "min1max2@tests.mozilla.org",
                                     "upgradeable1x2-3@tests.mozilla.org",
                                     "min1max3@tests.mozilla.org",
                                     "min1max3b@tests.mozilla.org",
                                     "override1x2-1x3@tests.mozilla.org"]);
  check_state_v2(addons);
});

// Upgrade to version 3 which will appDisable addons
// upgradeable1x2-3 and override1x2-1x3
// Only the newly disabled add-ons should be passed to the
// upgrade window
add_task(function* run_test_1() {
  gCheckUpdates = true;
  WindowWatcher.expected = true;

  yield promiseRestartManager("3");
  check_startup_changes("installed", []);
  check_startup_changes("updated", []);
  check_startup_changes("uninstalled", []);
  check_startup_changes("disabled", ["upgradeable1x2-3@tests.mozilla.org"]);
  check_startup_changes("enabled", []);
  do_check_false(WindowWatcher.expected);
  gCheckUpdates = false;

  let addons = yield promiseAddonsByIDs(["min1max1@tests.mozilla.org",
                                         "min1max2@tests.mozilla.org",
                                         "upgradeable1x2-3@tests.mozilla.org",
                                         "min1max3@tests.mozilla.org",
                                         "min1max3b@tests.mozilla.org",
                                         "override1x2-1x3@tests.mozilla.org"]);
  check_state_v3(addons);

  do_check_eq(WindowWatcher.args.length, 2);
  do_check_true(WindowWatcher.args.indexOf("upgradeable1x2-3@tests.mozilla.org") >= 0);
  do_check_true(WindowWatcher.args.indexOf("override1x2-1x3@tests.mozilla.org") >= 0);
});

// Downgrade to version 2 which will remove appDisable from two add-ons
// Still displays the compat window, because metadata is not recently updated
add_task(function* run_test_2() {
  WindowWatcher.expected = true;
  yield promiseRestartManager("2");
  check_startup_changes("installed", []);
  check_startup_changes("updated", []);
  check_startup_changes("uninstalled", []);
  check_startup_changes("disabled", []);
  check_startup_changes("enabled", ["upgradeable1x2-3@tests.mozilla.org"]);
  do_check_false(WindowWatcher.expected);

  let addons = yield promiseAddonsByIDs(["min1max1@tests.mozilla.org",
                                         "min1max2@tests.mozilla.org",
                                         "upgradeable1x2-3@tests.mozilla.org",
                                         "min1max3@tests.mozilla.org",
                                         "min1max3b@tests.mozilla.org",
                                         "override1x2-1x3@tests.mozilla.org"]);
  check_state_v2(addons);
});

// Upgrade back to version 3 which should only appDisable
// upgradeable1x2-3, because we already have the override
// stored in our DB for override1x2-1x3. Ensure that when
// the upgrade dialog updates an add-on no restart is necessary
add_task(function* run_test_5() {
  Services.prefs.setBoolPref(PREF_EM_SHOW_MISMATCH_UI, true);
  // tell the mock compatibility window to install the available upgrade
  gInstallUpdate = true;

  WindowWatcher.expected = true;
  yield promiseRestartManager("3");
  check_startup_changes("installed", []);
  check_startup_changes("updated", ["upgradeable1x2-3@tests.mozilla.org"]);
  check_startup_changes("uninstalled", []);
  check_startup_changes("disabled", []);
  check_startup_changes("enabled", []);
  do_check_false(WindowWatcher.expected);
  gInstallUpdate = false;

  let addons = yield promiseAddonsByIDs(["min1max1@tests.mozilla.org",
                                         "min1max2@tests.mozilla.org",
                                         "upgradeable1x2-3@tests.mozilla.org",
                                         "min1max3@tests.mozilla.org",
                                         "min1max3b@tests.mozilla.org",
                                         "override1x2-1x3@tests.mozilla.org"]);
  check_state_v3_2(addons);

  do_check_eq(WindowWatcher.args.length, 1);
  do_check_true(WindowWatcher.args.indexOf("upgradeable1x2-3@tests.mozilla.org") >= 0);
});

// Downgrade to version 1 which will appEnable all the add-ons
// except upgradeable1x2-3; the update we installed isn't compatible with 1
add_task(function* run_test_6() {
  WindowWatcher.expected = true;
  yield promiseRestartManager("1");
  check_startup_changes("installed", []);
  check_startup_changes("updated", []);
  check_startup_changes("uninstalled", []);
  check_startup_changes("disabled", ["upgradeable1x2-3@tests.mozilla.org"]);
  check_startup_changes("enabled", ["min1max1@tests.mozilla.org"]);
  do_check_false(WindowWatcher.expected);

  let addons = yield promiseAddonsByIDs(["min1max1@tests.mozilla.org",
                                         "min1max2@tests.mozilla.org",
                                         "upgradeable1x2-3@tests.mozilla.org",
                                         "min1max3@tests.mozilla.org",
                                         "min1max3b@tests.mozilla.org",
                                         "override1x2-1x3@tests.mozilla.org"]);
  check_state_v1_2(addons);
});

add_task(function* cleanup() {
  return new Promise((resolve, reject) => {
    testserver.stop(resolve);
  });
});

function run_test() {
  run_next_test();
}
