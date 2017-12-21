/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that the AddonManager objects cannot be tampered with

function run_test() {
  // Setup for test
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  startupManager();

  // Verify that properties cannot be changed
  let old = AddonManager.STATE_AVAILABLE;
  AddonManager.STATE_AVAILABLE = 28;
  Assert.equal(AddonManager.STATE_AVAILABLE, old);

  // Verify that functions cannot be replaced
  AddonManager.isInstallEnabled = function() {
    do_throw("Should not be able to replace a function");
  };
  AddonManager.isInstallEnabled("application/x-xpinstall");

  // Verify that properties cannot be added
  AddonManager.foo = "bar";
  Assert.equal(false, "foo" in AddonManager);
}
