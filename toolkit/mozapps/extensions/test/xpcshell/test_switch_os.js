/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

const ID = "bootstrap1@tests.mozilla.org";

BootstrapMonitor.init();

const profileDir = gProfD.clone();
profileDir.append("extensions");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

add_task(async function() {
  await promiseStartupManager();

  await promiseInstallFile(do_get_addon("test_bootstrap1_1"));

  let addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);

  BootstrapMonitor.checkAddonStarted(ID);
  Assert.ok(!addon.userDisabled);
  Assert.ok(addon.isActive);

  await promiseShutdownManager();

  BootstrapMonitor.checkAddonNotStarted(ID);

  let jData = await loadJSON(gExtensionsJSON.path);

  for (let addonInstance of jData.addons) {
    if (addonInstance.id == ID) {
      // Set to something that would be an invalid descriptor for this platform
      addonInstance.descriptor = AppConstants.platform == "win" ? "/foo/bar" : "C:\\foo\\bar";
    }
  }

  await saveJSON(jData, gExtensionsJSON.path);

  await promiseStartupManager();

  addon = await promiseAddonByID(ID);
  Assert.notEqual(addon, null);

  BootstrapMonitor.checkAddonStarted(ID);
  Assert.ok(!addon.userDisabled);
  Assert.ok(addon.isActive);
});
