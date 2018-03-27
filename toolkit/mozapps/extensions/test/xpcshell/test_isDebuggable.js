/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

var ADDONS = [
  "test_bootstrap2_1", // restartless addon
];

var IDS = [
  "bootstrap2@tests.mozilla.org",
];

function run_test() {
  do_test_pending();

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  startupManager();
  AddonManager.checkCompatibility = false;

  installAllFiles(ADDONS.map(do_get_addon), function() {
    restartManager();

    AddonManager.getAddonsByIDs(IDS, function([a1]) {
      Assert.equal(a1.isDebuggable, true);
      do_test_finished();
    });
  }, true);
}
