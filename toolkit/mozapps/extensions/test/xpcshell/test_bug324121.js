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
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

// Get the HTTP server.
do_load_httpd_js();
var testserver;

var next_test = null;
var gItemsNotChecked =[];

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
  pendingCount: 0,

  onUpdateAvailable: function onAddonUpdateEnded(aAddon) {
    switch (aAddon.id) {
      // add-on disabled - should not happen
      case "bug324121_1@tests.mozilla.org":
      // app id already compatible - should not happen
      case "bug324121_5@tests.mozilla.org":
      // toolkit id already compatible - should not happen
      case "bug324121_9@tests.mozilla.org":
        do_throw("Should not have seen an update check for " + aAddon.id);
        break;

      // app id incompatible update available
      case "bug324121_3@tests.mozilla.org":
      // update rdf not found
      case "bug324121_4@tests.mozilla.org":
      // toolkit id incompatible update available
      case "bug324121_7@tests.mozilla.org":
      // update rdf not found
      case "bug324121_8@tests.mozilla.org":
        do_throw("Should be no update available for " + aAddon.id);
        break;

      // Updates available
      case "bug324121_2@tests.mozilla.org":
      case "bug324121_6@tests.mozilla.org":
        break;

      default:
        do_throw("Update check for unknown " + aAddon.id);
    }

    // pos should always be >= 0 so just let this throw if this fails
    var pos = gItemsNotChecked.indexOf(aAddon.id);
    gItemsNotChecked.splice(pos, 1);
  },

  onNoUpdateAvailable: function onNoUpdateAvailable(aAddon) {
    switch (aAddon.id) {
      // add-on disabled - should not happen
      case "bug324121_1@tests.mozilla.org":
      // app id already compatible - should not happen
      case "bug324121_5@tests.mozilla.org":
      // toolkit id already compatible - should not happen
      case "bug324121_9@tests.mozilla.org":
        do_throw("Should not have seen an update check for " + aAddon.id);
        break;

      // app id incompatible update available
      case "bug324121_3@tests.mozilla.org":
      // update rdf not found
      case "bug324121_4@tests.mozilla.org":
      // toolkit id incompatible update available
      case "bug324121_7@tests.mozilla.org":
      // update rdf not found
      case "bug324121_8@tests.mozilla.org":
        break;

      // Updates available
      case "bug324121_2@tests.mozilla.org":
      case "bug324121_6@tests.mozilla.org":
        do_throw("Should be an update available for " + aAddon.id);
        break;

      default:
        do_throw("Update check for unknown " + aAddon.id);
    }

    // pos should always be >= 0 so just let this throw if this fails
    var pos = gItemsNotChecked.indexOf(aAddon.id);
    gItemsNotChecked.splice(pos, 1);
  },

  onUpdateFinished: function onUpdateFinished(aAddon) {
    if (--this.pendingCount == 0)
      test_complete();
  }
};

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  const dataDir = do_get_file("data");

  // Create and configure the HTTP server.
  testserver = new nsHttpServer();
  testserver.registerDirectory("/data/", dataDir);
  testserver.start(4444);

  startupManager();

  installAllFiles([do_get_addon(a.addon) for each (a in ADDONS)], function() {
    restartManager();
    AddonManager.getAddonByID(ADDONS[0].id, function(addon) {
      addon.userDisabled = true;
      restartManager();

      AddonManager.getAddonsByTypes(["extension"], function(installedItems) {
        var items = [];

        for (let k = 0; k < ADDONS.length; k++) {
          for (let i = 0; i < installedItems.length; i++) {
            if (ADDONS[k].id != installedItems[i].id)
              continue;
            if (installedItems[i].userDisabled)
              continue;

            if (ADDONS[k].shouldCheck == installedItems[i].isCompatibleWith("3", "3")) {
              do_throw(installedItems[i].id + " had the wrong compatibility: " +
                installedItems[i].isCompatibleWith("3", "3"));
            }

            if (ADDONS[k].shouldCheck) {
              gItemsNotChecked.push(ADDONS[k].id);
              updateListener.pendingCount++;
              installedItems[i].findUpdates(updateListener,
                                            AddonManager.UPDATE_WHEN_USER_REQUESTED,
                                            "3", "3");
            }
          }
        }
      });
    });
  });
}

function test_complete() {
  do_check_eq(gItemsNotChecked.length, 0);
  testserver.stop(do_test_finished);
}
