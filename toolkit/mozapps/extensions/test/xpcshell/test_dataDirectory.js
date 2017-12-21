/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Disables security checking our updates which haven't been signed
Services.prefs.setBoolPref("extensions.checkUpdateSecurity", false);

var ADDON = {
  id: "datadirectory1@tests.mozilla.org",
  addon: "test_data_directory"
};

function run_test() {
    var expectedDir = gProfD.clone();
    expectedDir.append("extension-data");
    expectedDir.append(ADDON.id);

    do_test_pending();
    Assert.ok(!expectedDir.exists());

    createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "1.9");
    startupManager();

    installAllFiles([do_get_addon(ADDON.addon)], function() {
        restartManager();

        AddonManager.getAddonByID(ADDON.id, function(item) {
            item.getDataDirectory(promise_callback);
        });
    });
}

function promise_callback() {
    Assert.equal(arguments.length, 2);
    var expectedDir = gProfD.clone();
    expectedDir.append("extension-data");
    expectedDir.append(ADDON.id);

    Assert.equal(arguments[0], expectedDir.path);
    Assert.ok(expectedDir.exists());
    Assert.ok(expectedDir.isDirectory());

    Assert.equal(arguments[1], null);

    // Cleanup.
    expectedDir.parent.remove(true);

    do_test_finished();
}
