/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that system addon about:config prefs
// are honored during startup/restarts/upgrades.
createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2");

let distroDir = FileUtils.getDir("ProfD", ["sysfeatures", "empty"], true);
registerDirectory("XREAppFeat", distroDir);

AddonTestUtils.usePrivilegedSignatures = "system";

add_task(initSystemAddonDirs);

BootstrapMonitor.init();

add_task(async function setup() {
  let xpi = await getSystemAddonXPI(1, "1.0");
  await AddonTestUtils.manuallyInstall(xpi, distroDir);
});

add_task(async function systemAddonPreffedOff() {
  const id = "system1@tests.mozilla.org";
  Services.prefs.setBoolPref("extensions.system1.enabled", false);

  await overrideBuiltIns({ system: [id] });

  await promiseStartupManager();

  BootstrapMonitor.checkInstalled(id);
  BootstrapMonitor.checkNotStarted(id);

  await promiseRestartManager();

  BootstrapMonitor.checkNotStarted(id);

  await promiseShutdownManager(false);
});

add_task(async function systemAddonPreffedOn() {
  const id = "system1@tests.mozilla.org";
  Services.prefs.setBoolPref("extensions.system1.enabled", true);

  await promiseStartupManager("2.0");

  BootstrapMonitor.checkInstalled(id);
  BootstrapMonitor.checkStarted(id);

  await promiseRestartManager();

  BootstrapMonitor.checkStarted(id);

  await promiseShutdownManager();
});
