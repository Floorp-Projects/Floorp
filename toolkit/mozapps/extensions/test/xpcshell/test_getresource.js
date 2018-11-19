/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies the functionality of getResourceURI
// There are two cases - with a filename it returns an nsIFileURL to the filename
// and with no parameters, it returns an nsIFileURL to the root of the addon

add_task(async function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  await promiseStartupManager();

  const ID = "addon@tests.mozilla.org";
  let xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: { applications: {gecko: {id: ID}}},
    files: {
      "icon.png": "Dummy icon file",
      "subdir/subfile.txt": "Dummy file in subdirectory",
    },
  });

  let install = await AddonManager.getInstallForFile(xpi);
  Assert.equal(install.addon.getResourceURI().spec, install.sourceURI.spec);

  Assert.equal(install.addon.getResourceURI("icon.png").spec,
               `jar:${install.sourceURI.spec}!/icon.png`);

  Assert.equal(install.addon.getResourceURI("subdir/subfile.txt").spec,
               `jar:${install.sourceURI.spec}!/subdir/subfile.txt`);

  await promiseCompleteInstall(install);
  await promiseRestartManager();
  let a1 = await AddonManager.getAddonByID(ID);
  Assert.notEqual(a1, null);

  let addonDir = gProfD.clone();
  addonDir.append("extensions");
  let rootUri = do_get_addon_root_uri(addonDir, ID);

  let uri = a1.getResourceURI("/");
  Assert.equal(uri.spec, rootUri);

  let file = rootUri + "manifest.json";
  uri = a1.getResourceURI("manifest.json");
  Assert.equal(uri.spec, file);

  file = rootUri + "icon.png";
  uri = a1.getResourceURI("icon.png");
  Assert.equal(uri.spec, file);

  file = rootUri + "subdir/subfile.txt";
  uri = a1.getResourceURI("subdir/subfile.txt");
  Assert.equal(uri.spec, file);

  await a1.uninstall();
});
