/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// install.rdf size, icon.png size, subfile.txt size
const ADDON_SIZE = 672 + 15 + 26;

// This verifies the functionality of getResourceURI
// There are two cases - with a filename it returns an nsIFileURL to the filename
// and with no parameters, it returns an nsIFileURL to the root of the addon

async function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  startupManager();

  let aInstall = await AddonManager.getInstallForFile(do_get_addon("test_getresource"));
  Assert.ok(aInstall.addon.hasResource("install.rdf"));
  Assert.equal(aInstall.addon.getResourceURI().spec, aInstall.sourceURI.spec);

  Assert.ok(aInstall.addon.hasResource("icon.png"));
  Assert.equal(aInstall.addon.getResourceURI("icon.png").spec,
               "jar:" + aInstall.sourceURI.spec + "!/icon.png");

  Assert.ok(!aInstall.addon.hasResource("missing.txt"));

  Assert.ok(aInstall.addon.hasResource("subdir/subfile.txt"));
  Assert.equal(aInstall.addon.getResourceURI("subdir/subfile.txt").spec,
               "jar:" + aInstall.sourceURI.spec + "!/subdir/subfile.txt");

  Assert.ok(!aInstall.addon.hasResource("subdir/missing.txt"));

  Assert.equal(aInstall.addon.size, ADDON_SIZE);

  await promiseCompleteAllInstalls([aInstall]);
  restartManager();
  let a1 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.notEqual(a1, null);

  let addonDir = gProfD.clone();
  addonDir.append("extensions");
  let rootUri = do_get_addon_root_uri(addonDir, "addon1@tests.mozilla.org");

  let uri = a1.getResourceURI("/");
  Assert.equal(uri.spec, rootUri);

  let file = rootUri + "install.rdf";
  Assert.ok(a1.hasResource("install.rdf"));
  uri = a1.getResourceURI("install.rdf");
  Assert.equal(uri.spec, file);

  file = rootUri + "icon.png";
  Assert.ok(a1.hasResource("icon.png"));
  uri = a1.getResourceURI("icon.png");
  Assert.equal(uri.spec, file);

  Assert.ok(!a1.hasResource("missing.txt"));

  file = rootUri + "subdir/subfile.txt";
  Assert.ok(a1.hasResource("subdir/subfile.txt"));
  uri = a1.getResourceURI("subdir/subfile.txt");
  Assert.equal(uri.spec, file);

  Assert.ok(!a1.hasResource("subdir/missing.txt"));

  Assert.equal(a1.size, ADDON_SIZE);

  a1.uninstall();

  try {
    // hasResource should never throw an exception.
    Assert.ok(!a1.hasResource("icon.png"));
  } catch (e) {
    Assert.ok(false);
  }

  let aInstall_2 = await AddonManager.getInstallForFile(do_get_addon("test_getresource"));
  Assert.ok(!a1.hasResource("icon.png"));
  Assert.ok(aInstall_2.addon.hasResource("icon.png"));

  restartManager();

  let newa1 = await AddonManager.getAddonByID("addon1@tests.mozilla.org");
  Assert.equal(newa1, null);

  executeSoon(do_test_finished);
}
