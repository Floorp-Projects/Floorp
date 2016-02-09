/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Components.utils.import("resource://gre/modules/AppConstants.jsm");

const ID = "bootstrap1@tests.mozilla.org";

BootstrapMonitor.init();

const profileDir = gProfD.clone();
profileDir.append("extensions");

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

add_task(function*() {
  startupManager();

  let install = yield new Promise(resolve => AddonManager.getInstallForFile(do_get_addon("test_bootstrap1_1"), resolve));
  yield promiseCompleteAllInstalls([install]);

  let addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);

  BootstrapMonitor.checkAddonStarted(ID);
  do_check_false(addon.userDisabled);
  do_check_true(addon.isActive);

  yield promiseShutdownManager();

  BootstrapMonitor.checkAddonNotStarted(ID);

  let jData = loadJSON(gExtensionsJSON);

  for (let addon of jData.addons) {
    if (addon.id == ID) {
      // Set to something that would be an invalid descriptor for this platform
      addon.descriptor = AppConstants.platform == "win" ? "/foo/bar" : "C:\\foo\\bar";
    }
  }

  saveJSON(jData, gExtensionsJSON);

  startupManager();

  addon = yield promiseAddonByID(ID);
  do_check_neq(addon, null);

  BootstrapMonitor.checkAddonStarted(ID);
  do_check_false(addon.userDisabled);
  do_check_true(addon.isActive);
});
