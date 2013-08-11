/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const ADDON = "test_bug521905";
const ID = "bug521905@tests.mozilla.org";

// Disables security checking our updates which haven't been signed
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

function run_test() {
  // This test is only relevant on builds where the version is included in the
  // checkCompatibility preference name
  if (isNightlyChannel()) {
    return;
  }

  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2.0pre", "2");

  startupManager();
  AddonManager.checkCompatibility = false;

  installAllFiles([do_get_addon(ADDON)], function() {
    restartManager();

    AddonManager.getAddonByID(ID, function(addon) {
      do_check_neq(addon, null);
      do_check_true(addon.isActive);

      do_execute_soon(run_test_1);
    });
  });
}

function run_test_1() {
  Services.prefs.setBoolPref("extensions.checkCompatibility.2.0pre", true);

  restartManager();
  AddonManager.getAddonByID(ID, function(addon) {
    do_check_neq(addon, null);
    do_check_false(addon.isActive);

    do_execute_soon(run_test_2);
  });
}

function run_test_2() {
  Services.prefs.setBoolPref("extensions.checkCompatibility.2.0p", false);

  restartManager();
  AddonManager.getAddonByID(ID, function(addon) {
    do_check_neq(addon, null);
    do_check_false(addon.isActive);

    do_execute_soon(do_test_finished);
  });
}
