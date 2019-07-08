/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that add-ons distributed with the application get installed
// correctly

// Allow distributed add-ons to install
Services.prefs.setBoolPref("extensions.installDistroAddons", true);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

const profileDir = gProfD.clone();
profileDir.append("extensions");
const distroDir = gProfD.clone();
distroDir.append("distribution");
distroDir.append("extensions");
registerDirectory("XREAppDist", distroDir.parent);

async function setOldModificationTime() {
  // Make sure the installed extension has an old modification time so any
  // changes will be detected
  await promiseShutdownManager();
  let extension = gProfD.clone();
  extension.append("extensions");
  extension.append(`${ID}.xpi`);
  setExtensionModifiedTime(extension, Date.now() - MAKE_FILE_OLD_DIFFERENCE);
  await promiseStartupManager();
}

const ID = "addon@tests.mozilla.org";

async function writeDistroAddon(version) {
  let xpi = await createTempWebExtensionFile({
    manifest: {
      version,
      applications: { gecko: { id: ID } },
    },
  });
  xpi.copyTo(distroDir, `${ID}.xpi`);
}

// Tests that on the first startup the add-on gets installed
add_task(async function run_test_1() {
  await writeDistroAddon("1.0");
  await promiseStartupManager();

  let a1 = await AddonManager.getAddonByID(ID);
  Assert.notEqual(a1, null);
  Assert.equal(a1.version, "1.0");
  Assert.ok(a1.isActive);
  Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);
  Assert.ok(!a1.foreignInstall);
});

// Tests that starting with a newer version in the distribution dir doesn't
// install it yet
add_task(async function run_test_2() {
  await setOldModificationTime();

  await writeDistroAddon("2.0");
  await promiseRestartManager();

  let a1 = await AddonManager.getAddonByID(ID);
  Assert.notEqual(a1, null);
  Assert.equal(a1.version, "1.0");
  Assert.ok(a1.isActive);
  Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);
});

// Test that an app upgrade installs the newer version
add_task(async function run_test_3() {
  await promiseRestartManager("2");

  let a1 = await AddonManager.getAddonByID(ID);
  Assert.notEqual(a1, null);
  Assert.equal(a1.version, "2.0");
  Assert.ok(a1.isActive);
  Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);
  Assert.ok(!a1.foreignInstall);
});

// Test that an app upgrade doesn't downgrade the extension
add_task(async function run_test_4() {
  await setOldModificationTime();

  await writeDistroAddon("1.0");
  await promiseRestartManager("3");

  let a1 = await AddonManager.getAddonByID(ID);
  Assert.notEqual(a1, null);
  Assert.equal(a1.version, "2.0");
  Assert.ok(a1.isActive);
  Assert.equal(a1.scope, AddonManager.SCOPE_PROFILE);
});

// Tests that after uninstalling a restart doesn't re-install the extension
add_task(async function run_test_5() {
  let a1 = await AddonManager.getAddonByID(ID);
  await a1.uninstall();

  await promiseRestartManager();

  let a1_2 = await AddonManager.getAddonByID(ID);
  Assert.equal(a1_2, null);
});

// Tests that upgrading the application still doesn't re-install the uninstalled
// extension
add_task(async function run_test_6() {
  await promiseRestartManager("4");

  let a1 = await AddonManager.getAddonByID(ID);
  Assert.equal(a1, null);
});
