/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// We require signature checks for this test
Services.prefs.setBoolPref(PREF_XPI_SIGNATURES_REQUIRED, true);
gUseRealCertChecks = true;

const profileDir = gProfD.clone();
profileDir.append("extensions");

const ID = "bootstrap1@tests.mozilla.org";

add_task(async function() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  AddonTestUtils.manuallyInstall(do_get_addon("test_cache_certdb"), profileDir, ID);

  startupManager();

  // Force a rescan of signatures
  const { XPIProvider } = Components.utils.import("resource://gre/modules/addons/XPIProvider.jsm", {});
  await XPIProvider.verifySignatures();

  let addon = await AddonManager.getAddonByID(ID);
  Assert.equal(addon.signedState, AddonManager.SIGNEDSTATE_MISSING);
  Assert.ok(!addon.isActive);
  Assert.ok(addon.appDisabled);
});
