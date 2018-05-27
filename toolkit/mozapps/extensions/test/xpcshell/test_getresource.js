/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies the functionality of getResourceURI
// There are two cases - with a filename it returns an nsIFileURL to the filename
// and with no parameters, it returns an nsIFileURL to the root of the addon

async function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  await promiseStartupManager();

  let aInstall = await AddonManager.getInstallForFile(do_get_addon("test_getresource"));
  Assert.equal(aInstall.addon.getResourceURI().spec, aInstall.sourceURI.spec);

  Assert.equal(aInstall.addon.getResourceURI("icon.png").spec,
               "jar:" + aInstall.sourceURI.spec + "!/icon.png");

  Assert.equal(aInstall.addon.getResourceURI("subdir/subfile.txt").spec,
               "jar:" + aInstall.sourceURI.spec + "!/subdir/subfile.txt");

  await promiseCompleteAllInstalls([aInstall]);
  await promiseRestartManager();
  let a1 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(a1, null);

  let addonDir = gProfD.clone();
  addonDir.append("extensions");
  let rootUri = do_get_addon_root_uri(addonDir, "addon1@tests.mozilla.org");

  let uri = a1.getResourceURI("/");
  Assert.equal(uri.spec, rootUri);

  let file = rootUri + "install.rdf";
  uri = a1.getResourceURI("install.rdf");
  Assert.equal(uri.spec, file);

  file = rootUri + "icon.png";
  uri = a1.getResourceURI("icon.png");
  Assert.equal(uri.spec, file);

  file = rootUri + "subdir/subfile.txt";
  uri = a1.getResourceURI("subdir/subfile.txt");
  Assert.equal(uri.spec, file);

  await a1.uninstall();

  await promiseRestartManager();

  let newa1 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.equal(newa1, null);

  executeSoon(do_test_finished);
}
