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
    do_check_neq(a, null);
    do_check_eq(a.version, "1.0");
    do_check_false(a.userDisabled);
    do_check_true(a.appDisabled);
    do_check_false(a.isActive);
    do_check_false(isExtensionInAddonsList(profileDir, a.id));

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
      do_check_neq(a2, null);
      do_check_eq(a2.version, "2.0");
      do_check_false(a2.userDisabled);
      do_check_false(a2.appDisabled);
      do_check_true(a2.isActive);
      do_check_true(isExtensionInAddonsList(profileDir, a2.id));

      do_execute_soon(do_test_finished);
    });
  }));
}
