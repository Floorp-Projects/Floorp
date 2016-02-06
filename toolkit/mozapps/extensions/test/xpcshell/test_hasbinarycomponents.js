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
                  function() {

    restartManager();

    AddonManager.getAddonsByIDs(["addon1@tests.mozilla.org",
                                 "addon2@tests.mozilla.org",
                                 "addon3@tests.mozilla.org",
                                 "addon4@tests.mozilla.org",
                                 "addon5@tests.mozilla.org"],
                                function([a1, a2, a3, a4, a5]) {
      // addon1 has no binary components
      do_check_neq(a1, null);
      do_check_false(a1.userDisabled);
      do_check_false(a1.hasBinaryComponents);
      do_check_true(a1.isCompatible);
      do_check_false(a1.appDisabled);
      do_check_true(a1.isActive);
      do_check_true(isExtensionInAddonsList(profileDir, a1.id));

      // addon2 has a binary component, is compatible
      do_check_neq(a2, null);
      do_check_false(a2.userDisabled);
      do_check_true(a2.hasBinaryComponents);
      do_check_true(a2.isCompatible);
      do_check_false(a2.appDisabled);
      do_check_true(a2.isActive);
      do_check_true(isExtensionInAddonsList(profileDir, a2.id));

      // addon3 has a binary component, is incompatible
      do_check_neq(a3, null);
      do_check_false(a3.userDisabled);
      do_check_true(a2.hasBinaryComponents);
      do_check_false(a3.isCompatible);
      do_check_true(a3.appDisabled);
      do_check_false(a3.isActive);
      do_check_false(isExtensionInAddonsList(profileDir, a3.id));

      // addon4 has a binary component listed in a sub-manifest, is incompatible
      do_check_neq(a4, null);
      do_check_false(a4.userDisabled);
      do_check_true(a2.hasBinaryComponents);
      do_check_false(a4.isCompatible);
      do_check_true(a4.appDisabled);
      do_check_false(a4.isActive);
      do_check_false(isExtensionInAddonsList(profileDir, a4.id));

      // addon5 has a binary component, but is set to not unpack
      do_check_neq(a5, null);
      do_check_false(a5.userDisabled);
      if (TEST_UNPACKED)
        do_check_true(a5.hasBinaryComponents);
      else
        do_check_false(a5.hasBinaryComponents);
      do_check_true(a5.isCompatible);
      do_check_false(a5.appDisabled);
      do_check_true(a5.isActive);
      do_check_true(isExtensionInAddonsList(profileDir, a5.id));

      do_execute_soon(do_test_finished);
    });
  });
}
