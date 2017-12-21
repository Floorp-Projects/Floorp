/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that attempts to override the global values fails but doesn't
// destroy the world with it
createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

const profileDir = gProfD.clone();
profileDir.append("extensions");

function getActiveVersion() {
  return Services.prefs.getIntPref("bootstraptest.active_version");
}

function getInstalledVersion() {
  return Services.prefs.getIntPref("bootstraptest.installed_version");
}

function run_test() {
  do_test_pending();

  manuallyInstall(do_get_addon("test_bug740612_1"), profileDir,
                  "bug740612_1@tests.mozilla.org");
  manuallyInstall(do_get_addon("test_bug740612_2"), profileDir,
                  "bug740612_2@tests.mozilla.org");

  startupManager();

  AddonManager.getAddonsByIDs(["bug740612_1@tests.mozilla.org",
                               "bug740612_2@tests.mozilla.org"],
                               function([a1, a2]) {
    Assert.notEqual(a1, null);
    Assert.notEqual(a2, null);
    Assert.equal(getInstalledVersion(), "1.0");
    Assert.equal(getActiveVersion(), "1.0");

    do_execute_soon(do_test_finished);
  });
}
