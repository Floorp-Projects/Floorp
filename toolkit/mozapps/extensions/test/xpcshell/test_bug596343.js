/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const URI_EXTENSION_SELECT_DIALOG     = "chrome://mozapps/content/extensions/selectAddons.xul";
const URI_EXTENSION_UPDATE_DIALOG     = "chrome://mozapps/content/extensions/update.xul";
const PREF_EM_SHOW_MISMATCH_UI        = "extensions.showMismatchUI";
const PREF_SHOWN_SELECTION_UI         = "extensions.shownSelectionUI";

Components.utils.import("resource://testing-common/MockRegistrar.jsm");

const profileDir = gProfD.clone();
profileDir.append("extensions");

var gExpectedURL = null;

// This will be called to show the any update dialog.
var WindowWatcher = {
  openWindow: function(parent, url, name, features, args) {
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

MockRegistrar.register("@mozilla.org/embedcomp/window-watcher;1", WindowWatcher);

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
      maxVersion: "2"
    }],
    name: "Test Addon 1",
  }, profileDir);

  // For a new profile it should disable showing the selection UI in the future
  // without showing the selection UI
  gExpectedURL = URI_EXTENSION_SELECT_DIALOG;
  startupManager();

  do_check_true(Services.prefs.getBoolPref(PREF_SHOWN_SELECTION_UI));
  do_check_eq(gExpectedURL, URI_EXTENSION_SELECT_DIALOG);

  // Reset the 'already shown' pref so that we can test that the first upgrade of
  // an existing profile shows the selection UI
  Services.prefs.clearUserPref(PREF_SHOWN_SELECTION_UI);

  restartManager("2");

  do_check_true(Services.prefs.getBoolPref(PREF_SHOWN_SELECTION_UI));
  do_check_eq(gExpectedURL, null);

  // Once we've seen the selection UI once, future upgrades will show the update dialog
  // but only if this upgrade disabled an add-on
  gExpectedURL = URI_EXTENSION_UPDATE_DIALOG;

  restartManager("3");

  do_check_eq(gExpectedURL, null);
}
