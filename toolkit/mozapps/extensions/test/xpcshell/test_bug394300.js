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
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

do_load_httpd_js();
var server;

// nsIAddonUpdateCheckListener implementation
var updateListener = {
  _count: 0,

  onUpdateAvailable: function onAddonUpdateEnded(aAddon, aInstall) {
    do_check_eq(aInstall.version, 10);
  },

  onNoUpdateAvailable: function onNoUpdateAvailable(aAddon) {
    do_throw("Expected an available update for " + aAddon.id);
  },

  onUpdateFinished: function onUpdateFinished() {
    if (++this._count == 2)
      server.stop(do_test_finished);
  },
}

function run_test()
{
  // Setup for test
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  startupManager();

  installAllFiles([do_get_addon("test_bug394300_1"),
                   do_get_addon("test_bug394300_2")], function() {

    restartManager();

    AddonManager.getAddonsByIDs(["bug394300_1@tests.mozilla.org",
                                 "bug394300_2@tests.mozilla.org"], function(updates) {

      do_check_neq(updates[0], null);
      do_check_neq(updates[1], null);

      server = new nsHttpServer();
      server.registerDirectory("/", do_get_file("data"));
      server.start(4444);

      updates[0].findUpdates(updateListener, AddonManager.UPDATE_WHEN_USER_REQUESTED);
      updates[1].findUpdates(updateListener, AddonManager.UPDATE_WHEN_USER_REQUESTED);
    });
  });
}
