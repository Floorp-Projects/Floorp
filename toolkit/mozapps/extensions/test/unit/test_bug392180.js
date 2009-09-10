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
 *      Dave Townsend <dtownsend@oxymoronical.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

// Disables security checking our updates which haven't been signed
gPrefs.setBoolPref("extensions.checkUpdateSecurity", false);

do_load_httpd_js();

var server;

var updateListener = {
  onUpdateStarted: function() {
  },

  onUpdateEnded: function() {
    gNext();
  },

  onAddonUpdateStarted: function(addon) {
  },

  onAddonUpdateEnded: function(addon, status) {
    // No update rdf will get found so this should be a failure.
    do_check_eq(status, Ci.nsIAddonUpdateCheckListener.STATUS_FAILURE);
  }
}

var requestHandler = {
  handle: function(metadata, response) {
    var updateType = metadata.queryString;
    do_check_eq(updateType, gType);
    response.setStatusLine(metadata.httpVersion, 404, "Not Found");
  }
}

var gAddon;
var gNext;
var gType;

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  startupEM();
  gEM.installItemFromFile(do_get_addon("test_bug392180"), NS_INSTALL_LOCATION_APPPROFILE);
  restartEM();

  gAddon = gEM.getItemForID("bug392180@tests.mozilla.org");
  do_check_neq(gAddon, null);

  server = new nsHttpServer();
  server.registerPathHandler("/update.rdf", requestHandler);
  server.start(4444);
  do_test_pending();

  run_test_1();
}

function end_test() {
  server.stop(do_test_finished);
}

function run_test_1() {
  // UPDATE_TYPE_COMPATIBILITY | UPDATE_TYPE_NEWVERSION;
  gType = 96;
  gNext = run_test_2;
  gEM.update([gAddon], 1, Ci.nsIExtensionManager.UPDATE_CHECK_NEWVERSION, updateListener);
}

function run_test_2() {
  // UPDATE_TYPE_COMPATIBILITY;
  gType = 32;
  gNext = run_test_3;
  gEM.update([gAddon], 1, Ci.nsIExtensionManager.UPDATE_CHECK_COMPATIBILITY, updateListener);
}

function run_test_3() {
  // UPDATE_TYPE_COMPATIBILITY | UPDATE_TYPE_NEWVERSION | UPDATE_WHEN_USER_REQUESTED;
  gType = 97;
  gNext = run_test_4;
  gEM.update([gAddon], 1, Ci.nsIExtensionManager.UPDATE_CHECK_NEWVERSION, updateListener,
             Ci.nsIExtensionManager.UPDATE_WHEN_USER_REQUESTED);
}

function run_test_4() {
  // UPDATE_TYPE_COMPATIBILITY | UPDATE_TYPE_NEWVERSION | UPDATE_WHEN_NEW_APP_DETECTED;
  gType = 98;
  gNext = run_test_5;
  gEM.update([gAddon], 1, Ci.nsIExtensionManager.UPDATE_CHECK_NEWVERSION, updateListener,
             Ci.nsIExtensionManager.UPDATE_WHEN_NEW_APP_DETECTED);
}

function run_test_5() {
  // UPDATE_TYPE_COMPATIBILITY | UPDATE_WHEN_NEW_APP_INSTALLED;
  gType = 35;
  gNext = run_test_6;
  gEM.update([gAddon], 1, Ci.nsIExtensionManager.UPDATE_CHECK_COMPATIBILITY, updateListener,
             Ci.nsIExtensionManager.UPDATE_WHEN_NEW_APP_INSTALLED);
}

function run_test_6() {
  // UPDATE_TYPE_COMPATIBILITY;
  gType = 35;
  gNext = end_test;
  try {
    gEM.update([gAddon], 1, Ci.nsIExtensionManager.UPDATE_CHECK_COMPATIBILITY, updateListener,
               16);
    do_throw("Should have thrown an exception");
  }
  catch (e) {
    do_check_eq(e.result, Components.results.NS_ERROR_ILLEGAL_VALUE);
    end_test();
  }
}
