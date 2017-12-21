/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests detection of binary components via parsing of chrome manifests.

const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2", "2");

  startupManager();

  installAllFiles([do_get_addon("test_chromemanifest_1"),
                   do_get_addon("test_chromemanifest_2"),
                   do_get_addon("test_chromemanifest_3"),
                   do_get_addon("test_chromemanifest_4"),
                   do_get_addon("test_chromemanifest_5")],
                  async function() {

    await promiseRestartManager();

    AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                 "addon2@tests.mozilla.org",
                                 "addon3@tests.mozilla.org",
                                 "addon4@tests.mozilla.org",
                                 "addon5@tests.mozilla.org"],
                                async function([a1, a2, a3, a4, a5]) {
      // addon1 has no binary components
      Assert.notEqual(a1, null);
      Assert.ok(!a1.userDisabled);
      Assert.ok(!a1.hasBinaryComponents);
      Assert.ok(a1.isCompatible);
      Assert.ok(!a1.appDisabled);
      Assert.ok(a1.isActive);
      Assert.ok(isExtensionInAddonsList(profileDir, a1.id));

      // addon2 has a binary component, is compatible
      Assert.notEqual(a2, null);
      Assert.ok(!a2.userDisabled);
      Assert.ok(a2.hasBinaryComponents);
      Assert.ok(a2.isCompatible);
      Assert.ok(!a2.appDisabled);
      Assert.ok(a2.isActive);
      Assert.ok(isExtensionInAddonsList(profileDir, a2.id));

      // addon3 has a binary component, is incompatible
      Assert.notEqual(a3, null);
      Assert.ok(!a3.userDisabled);
      Assert.ok(a2.hasBinaryComponents);
      Assert.ok(!a3.isCompatible);
      Assert.ok(a3.appDisabled);
      Assert.ok(!a3.isActive);
      Assert.ok(!isExtensionInAddonsList(profileDir, a3.id));

      // addon4 has a binary component listed in a sub-manifest, is incompatible
      Assert.notEqual(a4, null);
      Assert.ok(!a4.userDisabled);
      Assert.ok(a2.hasBinaryComponents);
      Assert.ok(!a4.isCompatible);
      Assert.ok(a4.appDisabled);
      Assert.ok(!a4.isActive);
      Assert.ok(!isExtensionInAddonsList(profileDir, a4.id));

      // addon5 has a binary component, but is set to not unpack
      Assert.notEqual(a5, null);
      Assert.ok(!a5.userDisabled);
      if (TEST_UNPACKED)
        Assert.ok(a5.hasBinaryComponents);
      else
        Assert.ok(!a5.hasBinaryComponents);
      Assert.ok(a5.isCompatible);
      Assert.ok(!a5.appDisabled);
      Assert.ok(a5.isActive);
      Assert.ok(isExtensionInAddonsList(profileDir, a5.id));

      do_execute_soon(do_test_finished);
    });
  });
}
