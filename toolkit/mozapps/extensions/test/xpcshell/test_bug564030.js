/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that upgrading an incompatible add-on to a compatible one forces an
// EM restart

const profileDir = gProfD.clone();
profileDir.append("extensions");

async function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "1.9.2");

  var dest = writeInstallRDFForExtension({
    id: "addon1@tests.mozilla.org",
    version: "1.0",
    name: "Test",
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "1",
      maxVersion: "1"
    }]
  }, profileDir);
  // Attempt to make this look like it was added some time in the past so
  // the update makes the last modified time change.
  setExtensionModifiedTime(dest, dest.lastModifiedTime - 5000);

  await promiseStartupManager();

  AddonManager.getAddonByID("addon1@tests.mozilla.org", callback_soon(async function(a) {
    Assert.notEqual(a, null);
    Assert.equal(a.version, "1.0");
    Assert.ok(!a.userDisabled);
    Assert.ok(a.appDisabled);
    Assert.ok(!a.isActive);
    Assert.ok(!isExtensionInAddonsList(profileDir, a.id));

    writeInstallRDFForExtension({
      id: "addon1@tests.mozilla.org",
      version: "2.0",
      name: "Test",
      targetApplications: [{
        id: "xpcshell@tests.mozilla.org",
        minVersion: "1",
        maxVersion: "2"
      }]
    }, profileDir);

    await promiseRestartManager();

    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a2) {
      Assert.notEqual(a2, null);
      Assert.equal(a2.version, "2.0");
      Assert.ok(!a2.userDisabled);
      Assert.ok(!a2.appDisabled);
      Assert.ok(a2.isActive);
      Assert.ok(isExtensionInAddonsList(profileDir, a2.id));

      executeSoon(do_test_finished);
    });
  }));
}
