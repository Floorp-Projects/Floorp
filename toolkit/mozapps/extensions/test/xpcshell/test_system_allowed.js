// Tests that only allowed built-in system add-ons are loaded on startup.

BootstrapMonitor.init();

// Build the test set
var distroDir = FileUtils.getDir("ProfD", ["sysfeatures"], true);
do_get_file("data/system_addons/system1_1.xpi").copyTo(distroDir, "system1@tests.mozilla.org.xpi");
do_get_file("data/system_addons/system2_1.xpi").copyTo(distroDir, "system2@tests.mozilla.org.xpi");
do_get_file("data/system_addons/system3_1.xpi").copyTo(distroDir, "system3@tests.mozilla.org.xpi");
registerDirectory("XREAppFeat", distroDir);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "0");

// Ensure that only allowed add-ons are loaded.
add_task(async function test_allowed_addons() {
  // 1 and 2 are allowed, 3 is not.
  let validAddons = { "system": ["system1@tests.mozilla.org", "system2@tests.mozilla.org"]};
  await overrideBuiltIns(validAddons);

  await promiseStartupManager();

  let addon = await AddonManager.getAddonByID("system1@tests.mozilla.org");
  notEqual(addon, null);

  addon = await AddonManager.getAddonByID("system2@tests.mozilla.org");
  notEqual(addon, null);

  addon = await AddonManager.getAddonByID("system3@tests.mozilla.org");
  Assert.equal(addon, null);
  equal(addon, null);

  // 3 is now allowed, 1 and 2 are not.
  validAddons = { "system": ["system3@tests.mozilla.org"]};
  await overrideBuiltIns(validAddons);

  await promiseRestartManager();

  addon = await AddonManager.getAddonByID("system1@tests.mozilla.org");
  equal(addon, null);

  addon = await AddonManager.getAddonByID("system2@tests.mozilla.org");
  equal(addon, null);

  addon = await AddonManager.getAddonByID("system3@tests.mozilla.org");
  notEqual(addon, null);

  await promiseShutdownManager();
});
