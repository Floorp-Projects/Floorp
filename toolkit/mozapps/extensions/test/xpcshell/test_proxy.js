/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const ID = "proxy1@tests.mozilla.org";

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

BootstrapMonitor.init();

// Ensure that a proxy file to an add-on with a valid manifest works.
add_task(async function() {
  Services.prefs.setBoolPref(PREF_XPI_SIGNATURES_REQUIRED, false);

  await promiseStartupManager();

  let tempdir = gTmpD.clone();
  await promiseWriteInstallRDFToDir({
    id: ID,
    version: "1.0",
    bootstrap: true,
    unpack: true,
    targetApplications: [{
          id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
        }],
    name: "Test Bootstrap 1 (proxy)",
  }, tempdir, ID, "bootstrap.js");

  let unpackedAddon = tempdir.clone();
  unpackedAddon.append(ID);
  do_get_file("data/test_proxy/bootstrap.js")
    .copyTo(unpackedAddon, "bootstrap.js");

  // create proxy file in profile/extensions dir
  let extensionsDir = gProfD.clone();
  extensionsDir.append("extensions");
  let proxyFile = await promiseWriteProxyFileToDir(extensionsDir, unpackedAddon, ID);

  await promiseRestartManager();

  BootstrapMonitor.checkAddonInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonStarted(ID, "1.0");

  let addon = await promiseAddonByID(ID);

  Assert.notEqual(addon, null);
  Assert.equal(addon.version, "1.0");
  Assert.equal(addon.name, "Test Bootstrap 1 (proxy)");
  Assert.ok(addon.isCompatible);
  Assert.ok(!addon.appDisabled);
  Assert.ok(addon.isActive);
  Assert.equal(addon.type, "extension");
  Assert.equal(addon.signedState, mozinfo.addon_signing ? AddonManager.SIGNEDSTATE_UNKNOWN : AddonManager.SIGNEDSTATE_NOT_REQUIRED);

  Assert.ok(proxyFile.exists());

  addon.uninstall();
  unpackedAddon.remove(true);

  await promiseRestartManager();
});


// Ensure that a proxy file to an add-on is not removed even
// if the manifest file is invalid. See bug 1195353.
add_task(async function() {
  let tempdir = gTmpD.clone();

  // use a mismatched ID to make this install.rdf invalid
  await promiseWriteInstallRDFToDir({
    id: "bad-proxy1@tests.mozilla.org",
    version: "1.0",
    bootstrap: true,
    unpack: true,
    targetApplications: [{
          id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
        }],
    name: "Test Bootstrap 1 (proxy)",
  }, tempdir, ID, "bootstrap.js");

  let unpackedAddon = tempdir.clone();
  unpackedAddon.append(ID);
  do_get_file("data/test_proxy/bootstrap.js")
    .copyTo(unpackedAddon, "bootstrap.js");

  // create proxy file in profile/extensions dir
  let extensionsDir = gProfD.clone();
  extensionsDir.append("extensions");
  let proxyFile = await promiseWriteProxyFileToDir(extensionsDir, unpackedAddon, ID);

  await promiseRestartManager();

  BootstrapMonitor.checkAddonNotInstalled(ID, "1.0");
  BootstrapMonitor.checkAddonNotStarted(ID, "1.0");

  let addon = await promiseAddonByID(ID);
  Assert.equal(addon, null);

  Assert.ok(proxyFile.exists());

  unpackedAddon.remove(true);
  proxyFile.remove(true);

  await promiseRestartManager();
});
