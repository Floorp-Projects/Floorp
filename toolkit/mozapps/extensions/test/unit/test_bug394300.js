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

// Disables security checking our updates which haven't been signed
gPrefs.setBoolPref("extensions.checkUpdateSecurity", false);

do_import_script("netwerk/test/httpserver/httpd.js");
var server;

// nsIAddonUpdateCheckListener implementation
var updateListener = {
  _count: 0,
  
  onUpdateStarted: function onUpdateStarted()
  {
  },

  onUpdateEnded: function onUpdateEnded()
  {
    server.stop();
    do_test_finished();
    do_check_eq(this._count, 2);
  },

  onAddonUpdateStarted: function onAddonUpdateStarted(aAddon)
  {
  },

  onAddonUpdateEnded: function onAddonUpdateEnded(aAddon, aStatus)
  {
    this._count++;
    do_check_eq(aStatus, Components.interfaces.nsIAddonUpdateCheckListener.STATUS_UPDATE);
    do_check_eq(aAddon.version, 10);
  }
}

function run_test()
{
  // Setup for test
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  
  startupEM();
  
  gEM.installItemFromFile(do_get_addon("test_bug394300_1"),  NS_INSTALL_LOCATION_APPPROFILE);
  gEM.installItemFromFile(do_get_addon("test_bug394300_2"),  NS_INSTALL_LOCATION_APPPROFILE);
  
  restartEM();
  
  var updates = [
    gEM.getItemForID("bug394300_1@tests.mozilla.org"),
    gEM.getItemForID("bug394300_2@tests.mozilla.org")
  ];
  
  do_check_neq(updates[0], null);
  do_check_neq(updates[1], null);
  
  server = new nsHttpServer();
  server.registerDirectory("/", do_get_file("toolkit/mozapps/extensions/test/unit/data"));
  server.start(4444);
  
  do_test_pending();
  
  gEM.update(updates, updates.length,
             Components.interfaces.nsIExtensionManager.UPDATE_CHECK_NEWVERSION,
             updateListener);
}
