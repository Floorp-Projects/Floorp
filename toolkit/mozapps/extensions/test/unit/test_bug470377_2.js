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
 * Dave Townsend <dtownsend@oxymoronical.com>.
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
// Disables compatibility checking
gPrefs.setBoolPref("extensions.checkCompatibility", false);

// This allows the EM to attempt to display errors to the user without failing
var promptService = {
  alert: function(aParent, aDialogTitle, aText) {
  },

  alertCheck: function(aParent, aDialogTitle, aText, aCheckMsg, aCheckState) {
  },

  confirm: function(aParent, aDialogTitle, aText) {
  },

  confirmCheck: function(aParent, aDialogTitle, aText, aCheckMsg, aCheckState) {
  },

  confirmEx: function(aParent, aDialogTitle, aText, aButtonFlags, aButton0Title, aButton1Title, aButton2Title, aCheckMsg, aCheckState) {
  },

  prompt: function(aParent, aDialogTitle, aText, aValue, aCheckMsg, aCheckState) {
  },

  promptUsernameAndPassword: function(aParent, aDialogTitle, aText, aUsername, aPassword, aCheckMsg, aCheckState) {
  },

  promptPassword: function(aParent, aDialogTitle, aText, aPassword, aCheckMsg, aCheckState) {
  },

  select: function(aParent, aDialogTitle, aText, aCount, aSelectList, aOutSelection) {
  },

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIPromptService)
     || iid.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

var PromptServiceFactory = {
  createInstance: function (outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return promptService.QueryInterface(iid);
  }
};
var registrar = Components.manager.QueryInterface(Components.interfaces.nsIComponentRegistrar);
registrar.registerFactory(Components.ID("{6cc9c9fe-bc0b-432b-a410-253ef8bcc699}"),
                          "PromptService",
                          "@mozilla.org/embedcomp/prompt-service;1", PromptServiceFactory);

var ADDONS = [
  "test_bug470377_1",
  "test_bug470377_2",
  "test_bug470377_3",
  "test_bug470377_4",
  "test_bug470377_5",
];

var InstallListener = {
  onDownloadStarted: function(aAddon) {
  },

  onDownloadProgress: function onProgress(aAddon, aValue, aMaxValue) {
  },

  onDownloadEnded: function(aAddon) {
  },

  onInstallStarted: function(aAddon) {
  },

  onCompatibilityCheckStarted: function(aAddon) {
    do_throw("Shouldn't perform any compatibility checks but did for " + aAddon.id);
  },

  onCompatibilityCheckEnded: function(aAddon, aStatus) {
  },

  onInstallEnded: function(aAddon, aStatus) {
  },

  onInstallsCompleted: function() {
    if (ADDONS.length == 0) {
      do_execute_soon(check_test);
      return;
    }
    gEM.installItemFromFile(do_get_addon(ADDONS.shift()), NS_INSTALL_LOCATION_APPPROFILE);
  }
};

do_load_httpd_js();
var server;

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  server = new nsHttpServer();
  server.registerDirectory("/", do_get_file("data/test_bug470377"));
  server.start(4444);

  do_test_pending();
  startupEM();
  gEM.addInstallListener(InstallListener);
  gEM.installItemFromFile(do_get_addon(ADDONS.shift()), NS_INSTALL_LOCATION_APPPROFILE);
}

function check_test() {
  restartEM();
  do_check_eq(gEM.getItemForID("bug470377_1@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("bug470377_2@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("bug470377_3@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("bug470377_4@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("bug470377_5@tests.mozilla.org"), null);
  do_test_finished();
}
