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
 * Robert Strong <robert.bugzilla@gmail.com>
 *
 * Portions created by the Initial Developer are Copyright (C) 2008
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

// Get the HTTP server.
do_import_script("netwerk/test/httpserver/httpd.js");
var testserver;

var next_test = null;

var gItemsNotCheck = [];

var ADDONS = [ {id: "bug324121_1@tests.mozilla.org",
                addon: "test_bug324121_1",
                shouldCheck: false },
               {id: "bug324121_2@tests.mozilla.org",
                addon: "test_bug324121_2",
                shouldCheck: true },
               {id: "bug324121_3@tests.mozilla.org",
                addon: "test_bug324121_3",
                shouldCheck: true },
               {id: "bug324121_4@tests.mozilla.org",
                addon: "test_bug324121_4",
                shouldCheck: true },
               {id: "bug324121_5@tests.mozilla.org",
                addon: "test_bug324121_5",
                shouldCheck: false },
               {id: "bug324121_6@tests.mozilla.org",
                addon: "test_bug324121_6",
                shouldCheck: true },
               {id: "bug324121_7@tests.mozilla.org",
                addon: "test_bug324121_7",
                shouldCheck: true },
               {id: "bug324121_8@tests.mozilla.org",
                addon: "test_bug324121_8",
                shouldCheck: true },
               {id: "bug324121_9@tests.mozilla.org",
                addon: "test_bug324121_9",
                shouldCheck: false } ];

// nsIAddonUpdateCheckListener
var updateListener = {
  onUpdateStarted: function onUpdateStarted() {
  },

  onUpdateEnded: function onUpdateEnded() {
    // Verify that all items that should be checked have been checked.
    do_check_eq(gItemsNotCheck.length, 0);
    test_complete();
  },

  onAddonUpdateStarted: function onAddonUpdateStarted(aAddon) {
  },

  onAddonUpdateEnded: function onAddonUpdateEnded(aAddon, aStatus) {
    var nsIAddonUpdateCheckListener = Ci.nsIAddonUpdateCheckListener;
    switch (aAddon.id)
    {
      case "bug324121_1@tests.mozilla.org":
        // add-on disabled - should not happen
        do_throw("Update check for disabled add-on " + aAddon.id);
        break;
      case "bug324121_2@tests.mozilla.org":
        // app id compatible update available
        do_check_eq(aStatus, nsIAddonUpdateCheckListener.STATUS_UPDATE);
        break;
      case "bug324121_3@tests.mozilla.org":
        // app id incompatible update available
        do_check_eq(aStatus, nsIAddonUpdateCheckListener.STATUS_NO_UPDATE);
        break;
      case "bug324121_4@tests.mozilla.org":
        // update rdf not found
        do_check_eq(aStatus, nsIAddonUpdateCheckListener.STATUS_FAILURE);
        break;
      case "bug324121_5@tests.mozilla.org":
        // app id already compatible - should not happen
        do_throw("Update check for compatible add-on " + aAddon.id);
        break;
      case "bug324121_6@tests.mozilla.org":
        // toolkit id compatible update available
        do_check_eq(aStatus, nsIAddonUpdateCheckListener.STATUS_UPDATE);
        break;
      case "bug324121_7@tests.mozilla.org":
        // toolkit id incompatible update available
        do_check_eq(aStatus, nsIAddonUpdateCheckListener.STATUS_NO_UPDATE);
        break;
      case "bug324121_8@tests.mozilla.org":
        // update rdf not found
        do_check_eq(aStatus, nsIAddonUpdateCheckListener.STATUS_FAILURE);
        break;
      case "bug324121_9@tests.mozilla.org":
        // toolkit id already compatible - should not happen
        do_throw("Update check for compatible add-on " + aAddon.id);
        break;
      default:
        do_throw("Update check for unknown " + aAddon.id);
    }

    // pos should always be >= 0 so just let this throw if this fails
    var pos = gItemsNotCheck.indexOf(aAddon.id);
    gItemsNotCheck.splice(pos, 1);
  }
};

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  const dataDir = do_get_file("toolkit/mozapps/extensions/test/unit/data");

  // Create and configure the HTTP server.
  testserver = new nsHttpServer();
  testserver.registerDirectory("/data/", dataDir);
  testserver.start(4444);

  startupEM();

  var addons = ["test_bug324121_1"];
  for (var k in ADDONS)
    gEM.installItemFromFile(do_get_addon(ADDONS[k].addon),
                            NS_INSTALL_LOCATION_APPPROFILE);

  restartEM();
  gEM.disableItem(ADDONS[0].id);
  restartEM();

  var items = gEM.getIncompatibleItemList("", "3", "3", Ci.nsIUpdateItem.TYPE_ANY,
                                          false, { });

  // Verify only items incompatible with the next app version are returned
  for (var k in ADDONS) {
    var found = false;
    for (var i = 0; i < items.length; ++i) {
      if (ADDONS[k].id == items[i].id) {
        gItemsNotCheck.push(items[i].id);
        found = true;
        break;
      }
    }
    do_check_true(ADDONS[k].shouldCheck == found);
  }

  gEM.update(items, items.length, Ci.nsIExtensionManager.UPDATE_NOTIFY_NEWVERSION,
             updateListener, "3", "3");

  do_test_pending();
}

function test_complete() {
  testserver.stop();
  do_test_finished();
}
