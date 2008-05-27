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
 * Portions created by the Initial Developer are Copyright (C) 2007
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
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 */

/**
 * Tests for update security restrictions.
 *
 * Install tests:
 *
 * Test     updateKey   updateURL   expected
 *-------------------------------------------
 * 1        absent      absent      success
 * 2        absent      http        failure
 * 3        absent      https       success
 * 4        present     absent      success
 * 5        present     http        success
 * 6        present     https       success
 *
 * Update tests:
 *
 * Test     signature   updateHash  updateLink   expected
 *--------------------------------------------------------
 * 5        absent      absent      http         fail
 * 7        broken      absent      http         fail
 * 8        correct     absent      http         no update
 * 9        correct     sha1        http         update
 * 10       corrent     absent      https        update
 * 11       corrent     sha1        https        update
 * 12       corrent     md2         http         no update
 * 13       corrent     md2         https        update
 */

do_import_script("netwerk/test/httpserver/httpd.js");
var server;

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
registrar.registerFactory(Components.ID("{6cc9c9fe-bc0b-432b-a410-253ef8bcc699}"), "PromptService",
                          "@mozilla.org/embedcomp/prompt-service;1", PromptServiceFactory);


var updateListener = {
onUpdateStarted: function()
{
},

onUpdateEnded: function()
{
  server.stop();
  do_test_finished();
},

onAddonUpdateStarted: function(addon)
{
},

onAddonUpdateEnded: function(addon, status)
{
  var nsIAddonUpdateCheckListener = Components.interfaces.nsIAddonUpdateCheckListener;
  switch (addon.id)
  {
    case "test_bug378216_5@tests.mozilla.org":
      // Update has no signature
      do_check_eq(status, nsIAddonUpdateCheckListener.STATUS_FAILURE);
      break;
    case "test_bug378216_7@tests.mozilla.org":
      // Update has a signature that does not match
      do_check_eq(status, nsIAddonUpdateCheckListener.STATUS_FAILURE);
      break;
    case "test_bug378216_8@tests.mozilla.org":
      // Update has a signature but no secure update
      do_check_eq(status, nsIAddonUpdateCheckListener.STATUS_NO_UPDATE);
      break;
    case "test_bug378216_9@tests.mozilla.org":
      // Update has a signature and a hashed update
      do_check_eq(status, nsIAddonUpdateCheckListener.STATUS_UPDATE);
      do_check_eq(addon.version, "2.0");
      break;
    case "test_bug378216_10@tests.mozilla.org":
      // Update has a signature and a https update
      do_check_eq(status, nsIAddonUpdateCheckListener.STATUS_UPDATE);
      do_check_eq(addon.version, "2.0");
      break;
    case "test_bug378216_11@tests.mozilla.org":
      // Update has a signature and a hashed https update
      do_check_eq(status, nsIAddonUpdateCheckListener.STATUS_UPDATE);
      do_check_eq(addon.version, "2.0");
      break;
    case "test_bug378216_12@tests.mozilla.org":
      // Update has a signature and an insecure hash against the update
      do_check_eq(status, nsIAddonUpdateCheckListener.STATUS_NO_UPDATE);
      break;
    case "test_bug378216_13@tests.mozilla.org":
      // The insecure hash doesn't matter if we have an https link
      do_check_eq(status, nsIAddonUpdateCheckListener.STATUS_UPDATE);
      do_check_eq(addon.version, "2.0");
      break;
    default:
      do_throw("Update check for unknown "+addon.id);
  }
}
}

function run_test()
{
  // Setup for test
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1");
  startupEM();
  gEM.installItemFromFile(do_get_addon("test_bug378216_1"), NS_INSTALL_LOCATION_APPPROFILE);
  gEM.installItemFromFile(do_get_addon("test_bug378216_2"), NS_INSTALL_LOCATION_APPPROFILE);
  gEM.installItemFromFile(do_get_addon("test_bug378216_3"), NS_INSTALL_LOCATION_APPPROFILE);
  gEM.installItemFromFile(do_get_addon("test_bug378216_4"), NS_INSTALL_LOCATION_APPPROFILE);
  gEM.installItemFromFile(do_get_addon("test_bug378216_5"), NS_INSTALL_LOCATION_APPPROFILE);
  gEM.installItemFromFile(do_get_addon("test_bug378216_6"), NS_INSTALL_LOCATION_APPPROFILE);
  gEM.installItemFromFile(do_get_addon("test_bug378216_7"), NS_INSTALL_LOCATION_APPPROFILE);
  gEM.installItemFromFile(do_get_addon("test_bug378216_8"), NS_INSTALL_LOCATION_APPPROFILE);
  gEM.installItemFromFile(do_get_addon("test_bug378216_9"), NS_INSTALL_LOCATION_APPPROFILE);
  gEM.installItemFromFile(do_get_addon("test_bug378216_10"), NS_INSTALL_LOCATION_APPPROFILE);
  gEM.installItemFromFile(do_get_addon("test_bug378216_11"), NS_INSTALL_LOCATION_APPPROFILE);
  gEM.installItemFromFile(do_get_addon("test_bug378216_12"), NS_INSTALL_LOCATION_APPPROFILE);
  gEM.installItemFromFile(do_get_addon("test_bug378216_13"), NS_INSTALL_LOCATION_APPPROFILE);
  
  restartEM();
  
  do_check_neq(gEM.getItemForID("test_bug378216_1@tests.mozilla.org"), null);
  // Test 2 has an insecure updateURL and no updateKey so should fail to install
  do_check_eq(gEM.getItemForID("test_bug378216_2@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("test_bug378216_3@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("test_bug378216_4@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("test_bug378216_5@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("test_bug378216_6@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("test_bug378216_7@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("test_bug378216_8@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("test_bug378216_9@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("test_bug378216_10@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("test_bug378216_11@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("test_bug378216_12@tests.mozilla.org"), null);
  do_check_neq(gEM.getItemForID("test_bug378216_13@tests.mozilla.org"), null);

  server = new nsHttpServer();
  server.registerDirectory("/", do_get_file("toolkit/mozapps/extensions/test/unit/data"));
  server.start(4444);
  
  var updates = [
    gEM.getItemForID("test_bug378216_5@tests.mozilla.org"),
    gEM.getItemForID("test_bug378216_7@tests.mozilla.org"),
    gEM.getItemForID("test_bug378216_8@tests.mozilla.org"),
    gEM.getItemForID("test_bug378216_9@tests.mozilla.org"),
    gEM.getItemForID("test_bug378216_10@tests.mozilla.org"),
    gEM.getItemForID("test_bug378216_11@tests.mozilla.org"),
    gEM.getItemForID("test_bug378216_12@tests.mozilla.org"),
    gEM.getItemForID("test_bug378216_13@tests.mozilla.org"),
  ];
  
  gEM.update(updates, updates.length,
             Components.interfaces.nsIExtensionManager.UPDATE_CHECK_NEWVERSION,
             updateListener);
  do_test_pending();
}
