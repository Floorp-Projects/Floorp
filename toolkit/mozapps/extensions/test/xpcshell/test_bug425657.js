/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const ADDON = "test_bug425657";
const ID = "bug425657@tests.mozilla.org";

function run_test() {
  // Setup for test
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1");

  // Install test add-on
  startupManager();
  installAllFiles([do_get_addon(ADDON)], function() {
    restartManager();
    AddonManager.getAddonByID(ID, function(addon) {
      do_check_neq(addon, null);
      do_check_eq(addon.name, "Deutsches W\u00f6rterbuch");
      do_check_eq(addon.name.length, 20);

      do_execute_soon(do_test_finished);
    });
  });
}
