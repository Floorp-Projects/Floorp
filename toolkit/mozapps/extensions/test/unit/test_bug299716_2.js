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
 * Dave Townsend <dtownsend@mozilla.com>.
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
 * the terms of any one of the MPL, the GPL or the LGPL
 *
 * ***** END LICENSE BLOCK *****
 */

// Update check listener.
const checkListener = {
  // nsIAddonUpdateCheckListener
  onUpdateStarted: function onUpdateStarted() {
  },

  // nsIAddonUpdateCheckListener
  onUpdateEnded: function onUpdateEnded() {
    var item = gEM.getItemForID(ADDON.id);
    do_check_eq(item.version, 0.1);
    do_check_eq(item.targetAppID, "xpcshell@tests.mozilla.org");
    do_check_eq(item.minAppVersion, 1);
    do_check_eq(item.maxAppVersion, 1);

    do_test_finished();

    testserver.stop();
  },

  // nsIAddonUpdateCheckListener
  onAddonUpdateStarted: function onAddonUpdateStarted(aAddon) {
  },

  // nsIAddonUpdateCheckListener
  onAddonUpdateEnded: function onAddonUpdateEnded(aAddon, aStatus) {
  }
}

// Get the HTTP server.
do_import_script("netwerk/test/httpserver/httpd.js");
var testserver;

var ADDON = {
  id: "bug299716-2@tests.mozilla.org",
  addon: "test_bug299716_2"
};

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  const dataDir = do_get_file("toolkit/mozapps/extensions/test/unit/data");
  const addonsDir = do_get_addon(ADDON.addon).parent;

  // Create and configure the HTTP server.
  testserver = new nsHttpServer();
  testserver.registerDirectory("/addons/", addonsDir);
  testserver.registerDirectory("/data/", dataDir);
  testserver.start(4444);

  startupEM();

  // Should just install.
  gEM.installItemFromFile(do_get_addon(ADDON.addon),
                          NS_INSTALL_LOCATION_APPPROFILE);
  restartEM();

  var item = gEM.getItemForID(ADDON.id);
  do_check_eq(item.version, 0.1);
  do_check_eq(item.targetAppID, "xpcshell@tests.mozilla.org");
  do_check_eq(item.minAppVersion, 1);
  do_check_eq(item.maxAppVersion, 1);

  do_test_pending();

  gEM.update([item], 1,
             Components.interfaces.nsIExtensionManager.UPDATE_SYNC_COMPATIBILITY,
             checkListener);
}
