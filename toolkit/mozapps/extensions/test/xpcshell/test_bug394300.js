/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Disables security checking our updates which haven't been signed
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

var server;

// nsIAddonUpdateCheckListener implementation
var updateListener = {
  _count: 0,

  onUpdateAvailable: function onAddonUpdateEnded(aAddon, aInstall) {
    Assert.equal(aInstall.version, 10);
  },

  onNoUpdateAvailable: function onNoUpdateAvailable(aAddon) {
    do_throw("Expected an available update for " + aAddon.id);
  },

  onUpdateFinished: function onUpdateFinished() {
    if (++this._count == 2)
      do_test_finished();
  },
};

function run_test() {
  // Setup for test
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  startupManager();

  installAllFiles([do_get_addon("test_bug394300_1"),
                   do_get_addon("test_bug394300_2")], function() {

    restartManager();

    AddonManager.getAddonsByIDs(["bug394300_1@tests.mozilla.org",
                                 "bug394300_2@tests.mozilla.org"], function(updates) {

      Assert.notEqual(updates[0], null);
      Assert.notEqual(updates[1], null);

      server = AddonTestUtils.createHttpServer({hosts: ["example.com"]});
      server.registerDirectory("/", do_get_file("data"));

      updates[0].findUpdates(updateListener, AddonManager.UPDATE_WHEN_USER_REQUESTED);
      updates[1].findUpdates(updateListener, AddonManager.UPDATE_WHEN_USER_REQUESTED);
    });
  });
}
