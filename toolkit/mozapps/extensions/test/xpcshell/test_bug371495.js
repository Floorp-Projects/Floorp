/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const PREF_MATCH_OS_LOCALE = "intl.locale.matchOS";
const PREF_SELECTED_LOCALE = "general.useragent.locale";

const ADDON = "test_bug371495";
const ID = "bug371495@tests.mozilla.org";

function run_test()
{
  // Setup for test
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1");

  // Install test add-on
  startupManager();
  installAllFiles([do_get_addon(ADDON)], function() {
    AddonManager.getAddonByID(ID, callback_soon(function(addon) {
      do_check_neq(addon, null);
      do_check_eq(addon.name, "Test theme");
      restartManager();

      AddonManager.getAddonByID(ID, callback_soon(function(addon) {
        do_check_neq(addon, null);
        do_check_eq(addon.optionsURL, null);
        do_check_eq(addon.aboutURL, null);

        do_execute_soon(do_test_finished);
      }));
    }));
  });
}
