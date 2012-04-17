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
 * Portions created by the Initial Developer are Copyright (C) 2010
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

const ADDON = "test_bug521905";
const ID = "bug521905@tests.mozilla.org";

// Disables security checking our updates which haven't been signed
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);
AddonManager.checkCompatibility = false;

function run_test() {
  var channel = "default";
  try {
    channel = Services.prefs.getCharPref("app.update.channel");
  }
  catch (e) { }

  // This test is only relevant on builds where the version is included in the
  // checkCompatibility preference name
  if (isNightlyChannel(channel)) {
    return;
  }

  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2.0pre", "2");

  startupManager();
  installAllFiles([do_get_addon(ADDON)], function() {
    restartManager();

    AddonManager.getAddonByID(ID, function(addon) {
      do_check_neq(addon, null);
      do_check_true(addon.isActive);

      run_test_1();
    });
  });
}

function run_test_1() {
  Services.prefs.setBoolPref("extensions.checkCompatibility.2.0pre", true);

  restartManager();
  AddonManager.getAddonByID(ID, function(addon) {
    do_check_neq(addon, null);
    do_check_false(addon.isActive);

    run_test_2();
  });
}

function run_test_2() {
  Services.prefs.setBoolPref("extensions.checkCompatibility.2.0p", false);

  restartManager();
  AddonManager.getAddonByID(ID, function(addon) {
    do_check_neq(addon, null);
    do_check_false(addon.isActive);

    do_test_finished();
  });
}
