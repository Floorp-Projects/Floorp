/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const URI_EXTENSION_SELECT_DIALOG     = "chrome://mozapps/content/extensions/selectAddons.xul";
const URI_EXTENSION_UPDATE_DIALOG     = "chrome://mozapps/content/extensions/update.xul";
const PREF_EM_SHOW_MISMATCH_UI        = "extensions.showMismatchUI";
const PREF_SHOWN_SELECTION_UI         = "extensions.shownSelectionUI";

const profileDir = gProfD.clone();
profileDir.append("extensions");

var gExpectedURL = null;

// This will be called to show the any update dialog.
var WindowWatcher = {
  openWindow: function(parent, url, name, features, arguments) {
    do_check_eq(url, gExpectedURL);
    gExpectedURL = null;
  },

  QueryInterface: function(iid) {
    if (iid.equals(AM_Ci.nsIWindowWatcher)
     || iid.equals(AM_Ci.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
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

// Tests that the selection UI is displayed when upgrading an existing profile
function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  Services.prefs.setBoolPref(PREF_EM_SHOW_MISMATCH_UI, true);

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

  startupManager();

  // For a new profile it should disable showing the selection UI in the future
  do_check_true(Services.prefs.getBoolPref(PREF_SHOWN_SELECTION_UI));
  Services.prefs.clearUserPref(PREF_SHOWN_SELECTION_UI);

  gExpectedURL = URI_EXTENSION_SELECT_DIALOG;
  restartManager("2");

  do_check_true(Services.prefs.getBoolPref(PREF_SHOWN_SELECTION_UI));
  do_check_eq(gExpectedURL, null);

  gExpectedURL = URI_EXTENSION_UPDATE_DIALOG;

  restartManager("3");

  do_check_eq(gExpectedURL, null);
}
