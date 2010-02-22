/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Townsend <dtownsend@oxymoronical.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL
 *
 * ***** END LICENSE BLOCK *****
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

const URI_EXTENSION_UPDATE_DIALOG     = "chrome://mozapps/content/extensions/update.xul";
const PREF_EM_DISABLED_ADDONS_LIST    = "extensions.disabledAddons";
const PREF_EM_SHOW_MISMATCH_UI        = "extensions.showMismatchUI";

// This will be called to show the blocklist message, we just make it look like
// it was dismissed.
var WindowWatcher = {
  expected: false,
  arguments: null,

  openWindow: function(parent, url, name, features, arguments) {
    do_check_eq(url, URI_EXTENSION_UPDATE_DIALOG);
    do_check_true(this.expected);
    this.expected = false;
    this.arguments = arguments.QueryInterface(Ci.nsIVariant);
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

function isDisabled(id) {
  return getManifestProperty(id, "isDisabled") == "true";
}

function appDisabled(id) {
  return getManifestProperty(id, "appDisabled") == "true";
}

function userDisabled(id) {
  return getManifestProperty(id, "userDisabled") == "true";
}

function check_state_v1() {
  do_check_neq(gEM.getItemForID("bug542391_1@tests.mozilla.org"), null);
  do_check_false(appDisabled("bug542391_1@tests.mozilla.org"));
  do_check_false(userDisabled("bug542391_1@tests.mozilla.org"));

  do_check_neq(gEM.getItemForID("bug542391_2@tests.mozilla.org"), null);
  do_check_false(appDisabled("bug542391_2@tests.mozilla.org"));
  do_check_true(userDisabled("bug542391_2@tests.mozilla.org"));

  do_check_neq(gEM.getItemForID("bug542391_3@tests.mozilla.org"), null);
  do_check_false(appDisabled("bug542391_3@tests.mozilla.org"));
  do_check_false(userDisabled("bug542391_3@tests.mozilla.org"));

  do_check_neq(gEM.getItemForID("bug542391_4@tests.mozilla.org"), null);
  do_check_false(appDisabled("bug542391_4@tests.mozilla.org"));
  do_check_true(userDisabled("bug542391_4@tests.mozilla.org"));

  do_check_neq(gEM.getItemForID("bug542391_5@tests.mozilla.org"), null);
  do_check_false(appDisabled("bug542391_5@tests.mozilla.org"));
  do_check_false(userDisabled("bug542391_5@tests.mozilla.org"));
}

function check_state_v2() {
  do_check_neq(gEM.getItemForID("bug542391_1@tests.mozilla.org"), null);
  do_check_true(appDisabled("bug542391_1@tests.mozilla.org"));
  do_check_false(userDisabled("bug542391_1@tests.mozilla.org"));

  do_check_neq(gEM.getItemForID("bug542391_2@tests.mozilla.org"), null);
  do_check_false(appDisabled("bug542391_2@tests.mozilla.org"));
  do_check_true(userDisabled("bug542391_2@tests.mozilla.org"));

  do_check_neq(gEM.getItemForID("bug542391_3@tests.mozilla.org"), null);
  do_check_false(appDisabled("bug542391_3@tests.mozilla.org"));
  do_check_false(userDisabled("bug542391_3@tests.mozilla.org"));

  do_check_neq(gEM.getItemForID("bug542391_4@tests.mozilla.org"), null);
  do_check_false(appDisabled("bug542391_4@tests.mozilla.org"));
  do_check_true(userDisabled("bug542391_4@tests.mozilla.org"));

  do_check_neq(gEM.getItemForID("bug542391_5@tests.mozilla.org"), null);
  do_check_false(appDisabled("bug542391_5@tests.mozilla.org"));
  do_check_false(userDisabled("bug542391_5@tests.mozilla.org"));
}

function check_state_v3() {
  do_check_neq(gEM.getItemForID("bug542391_1@tests.mozilla.org"), null);
  do_check_true(appDisabled("bug542391_1@tests.mozilla.org"));
  do_check_false(userDisabled("bug542391_1@tests.mozilla.org"));

  do_check_neq(gEM.getItemForID("bug542391_2@tests.mozilla.org"), null);
  do_check_true(appDisabled("bug542391_2@tests.mozilla.org"));
  do_check_true(userDisabled("bug542391_2@tests.mozilla.org"));

  do_check_neq(gEM.getItemForID("bug542391_3@tests.mozilla.org"), null);
  do_check_true(appDisabled("bug542391_3@tests.mozilla.org"));
  do_check_false(userDisabled("bug542391_3@tests.mozilla.org"));

  do_check_neq(gEM.getItemForID("bug542391_4@tests.mozilla.org"), null);
  do_check_false(appDisabled("bug542391_4@tests.mozilla.org"));
  do_check_true(userDisabled("bug542391_4@tests.mozilla.org"));

  do_check_neq(gEM.getItemForID("bug542391_5@tests.mozilla.org"), null);
  do_check_false(appDisabled("bug542391_5@tests.mozilla.org"));
  do_check_false(userDisabled("bug542391_5@tests.mozilla.org"));
}

// Install all the test add-ons, disable two of them and "upgrade" the app to
// version 2 which will appDisable one.
function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  startupEM();

  gEM.installItemFromFile(do_get_addon("test_bug542391_1"),
                          NS_INSTALL_LOCATION_APPPROFILE);
  gEM.installItemFromFile(do_get_addon("test_bug542391_2"),
                          NS_INSTALL_LOCATION_APPPROFILE);
  gEM.installItemFromFile(do_get_addon("test_bug542391_3"),
                          NS_INSTALL_LOCATION_APPPROFILE);
  gEM.installItemFromFile(do_get_addon("test_bug542391_4"),
                          NS_INSTALL_LOCATION_APPPROFILE);
  gEM.installItemFromFile(do_get_addon("test_bug542391_5"),
                          NS_INSTALL_LOCATION_APPPROFILE);

  restartEM();
  gEM.disableItem("bug542391_2@tests.mozilla.org");
  gEM.disableItem("bug542391_4@tests.mozilla.org");
  restartEM();

  check_state_v1();

  WindowWatcher.expected = true;
  restartEM("2");
  do_check_false(WindowWatcher.expected);
  check_state_v2();

  run_test_1();
}

// Upgrade to version 3 which will appDisable two more add-ons. Check that the
// 3 already disabled add-ons were passed to the mismatch dialog.
function run_test_1() {
  WindowWatcher.expected = true;
  restartEM("3");
  do_check_false(WindowWatcher.expected);
  check_state_v3();
  do_check_eq(WindowWatcher.arguments.length, 3);
  do_check_true(WindowWatcher.arguments.indexOf("bug542391_1@tests.mozilla.org") >= 0);
  do_check_true(WindowWatcher.arguments.indexOf("bug542391_2@tests.mozilla.org") >= 0);
  do_check_true(WindowWatcher.arguments.indexOf("bug542391_4@tests.mozilla.org") >= 0);

  run_test_2();
}

// Downgrade to version 2 which will remove appDisable from two add-ons and
// should pass all 4 previously disabled add-ons.
function run_test_2() {
  WindowWatcher.expected = true;
  restartEM("2");
  do_check_false(WindowWatcher.expected);
  check_state_v2();
  do_check_eq(WindowWatcher.arguments.length, 4);
  do_check_true(WindowWatcher.arguments.indexOf("bug542391_1@tests.mozilla.org") >= 0);
  do_check_true(WindowWatcher.arguments.indexOf("bug542391_2@tests.mozilla.org") >= 0);
  do_check_true(WindowWatcher.arguments.indexOf("bug542391_3@tests.mozilla.org") >= 0);
  do_check_true(WindowWatcher.arguments.indexOf("bug542391_4@tests.mozilla.org") >= 0);

  run_test_4();
}

// Upgrade to version 3 which will appDisable two more add-ons.
function run_test_3() {
  gPrefs.setBoolPref(PREF_EM_SHOW_MISMATCH_UI, false);

  restartEM("3");
  check_state_v3();
  var disabled = [];
  try {
    gPrefs.getCharPref(PREF_EM_DISABLED_ADDONS_LIST).split(",");
  }
  catch (e) {}
  do_check_eq(disabled.length, 2);
  do_check_true(disabled.indexOf("bug542391_2@tests.mozilla.org") >= 0);
  do_check_true(disabled.indexOf("bug542391_3@tests.mozilla.org") >= 0);
  gPrefs.clearUserPref(PREF_EM_DISABLED_ADDONS_LIST);

  run_test_2();
}

// Downgrade to version 2 which will remove appDisable from two add-ons.
function run_test_4() {
  restartEM("2");
  check_state_v2();
  var disabled = [];
  try {
    gPrefs.getCharPref(PREF_EM_DISABLED_ADDONS_LIST).split(",");
  }
  catch (e) {}
  do_check_eq(disabled.length, 0);

  finish_test();
}

function finish_test() {
  gEM.uninstallItem("bug542391_1@tests.mozilla.org");
  gEM.uninstallItem("bug542391_2@tests.mozilla.org");
  gEM.uninstallItem("bug542391_3@tests.mozilla.org");
  gEM.uninstallItem("bug542391_4@tests.mozilla.org");
  gEM.uninstallItem("bug542391_5@tests.mozilla.org");

  restartEM();
  do_check_eq(gEM.getItemForID("bug542391_1@tests.mozilla.org"), null);
  do_check_eq(gEM.getItemForID("bug542391_2@tests.mozilla.org"), null);
  do_check_eq(gEM.getItemForID("bug542391_3@tests.mozilla.org"), null);
  do_check_eq(gEM.getItemForID("bug542391_4@tests.mozilla.org"), null);
  do_check_eq(gEM.getItemForID("bug542391_5@tests.mozilla.org"), null);
}
