/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that trying to upgrade or uninstall an extension that has a file locked
// will roll back the upgrade or uninstall and retry at the next restart

const profileDir = gProfD.clone();
profileDir.append("extensions");

function run_test() {
  // This is only an issue on windows.
  if (!("nsIWindowsRegKey" in AM_Ci))
    return;

  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  startupManager();
  run_test_1();
}

function check_addon(aAddon, aVersion) {
  do_check_neq(aAddon, null);
  do_check_eq(aAddon.version, aVersion);
  do_check_true(aAddon.isActive);
  do_check_true(isExtensionInAddonsList(profileDir, aAddon.id));

  do_check_true(aAddon.hasResource("testfile"));
  if (aVersion == "1.0") {
    do_check_true(aAddon.hasResource("testfile1"));
    do_check_false(aAddon.hasResource("testfile2"));
  } else {
    do_check_false(aAddon.hasResource("testfile1"));
    do_check_true(aAddon.hasResource("testfile2"));
  }

  do_check_eq(aAddon.pendingOperations, AddonManager.PENDING_NONE);
}

function check_addon_upgrading(aAddon) {
  do_check_neq(aAddon, null);
  do_check_eq(aAddon.version, "1.0");
  do_check_true(aAddon.isActive);
  do_check_true(isExtensionInAddonsList(profileDir, aAddon.id));

  do_check_true(aAddon.hasResource("testfile"));
  do_check_true(aAddon.hasResource("testfile1"));
  do_check_false(aAddon.hasResource("testfile2"));

  do_check_eq(aAddon.pendingOperations, AddonManager.PENDING_UPGRADE);

  do_check_eq(aAddon.pendingUpgrade.version, "2.0");
}

function check_addon_uninstalling(aAddon, aAfterRestart) {
  do_check_neq(aAddon, null);
  do_check_eq(aAddon.version, "1.0");

  if (aAfterRestart) {
    do_check_false(aAddon.isActive);
    do_check_false(isExtensionInAddonsList(profileDir, aAddon.id));
  } else {
    do_check_true(aAddon.isActive);
    do_check_true(isExtensionInAddonsList(profileDir, aAddon.id));
  }

  do_check_true(aAddon.hasResource("testfile"));
  do_check_true(aAddon.hasResource("testfile1"));
  do_check_false(aAddon.hasResource("testfile2"));

  do_check_eq(aAddon.pendingOperations, AddonManager.PENDING_UNINSTALL);
}

function run_test_1() {
  installAllFiles([do_get_addon("test_bug587088_1")], function() {
    restartManager();

    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
      check_addon(a1, "1.0");

      // Lock either install.rdf for unpacked add-ons or the xpi for packed add-ons.
      let uri = a1.getResourceURI("install.rdf");
      if (uri.schemeIs("jar"))
        uri = a1.getResourceURI();

      let fstream = AM_Cc["@mozilla.org/network/file-input-stream;1"].
                    createInstance(AM_Ci.nsIFileInputStream);
      fstream.init(uri.QueryInterface(AM_Ci.nsIFileURL).file, -1, 0, 0);

      installAllFiles([do_get_addon("test_bug587088_2")], function() {

        check_addon_upgrading(a1);

        restartManager();

        AddonManager.getAddonByID("addon1@tests.mozilla.org", callback_soon(function(a1_2) {
          check_addon_upgrading(a1_2);

          restartManager();

          AddonManager.getAddonByID("addon1@tests.mozilla.org", callback_soon(function(a1_3) {
            check_addon_upgrading(a1_3);

            fstream.close();

            restartManager();

            AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1_4) {
              check_addon(a1_4, "2.0");

              a1_4.uninstall();
              do_execute_soon(run_test_2);
            });
          }));
        }));
      });
    });
  });
}

// Test that a failed uninstall gets rolled back
function run_test_2() {
  restartManager();

  installAllFiles([do_get_addon("test_bug587088_1")], async function() {
    await promiseRestartManager();

    AddonManager.getAddonByID("addon1@tests.mozilla.org", callback_soon(async function(a1) {
      check_addon(a1, "1.0");

      // Lock either install.rdf for unpacked add-ons or the xpi for packed add-ons.
      let uri = a1.getResourceURI("install.rdf");
      if (uri.schemeIs("jar"))
        uri = a1.getResourceURI();

      let fstream = AM_Cc["@mozilla.org/network/file-input-stream;1"].
                    createInstance(AM_Ci.nsIFileInputStream);
      fstream.init(uri.QueryInterface(AM_Ci.nsIFileURL).file, -1, 0, 0);

      a1.uninstall();

      check_addon_uninstalling(a1);

      await promiseRestartManager();

      AddonManager.getAddonByID("addon1@tests.mozilla.org", callback_soon(async function(a1_2) {
        check_addon_uninstalling(a1_2, true);

        await promiseRestartManager();

        AddonManager.getAddonByID("addon1@tests.mozilla.org", callback_soon(async function(a1_3) {
          check_addon_uninstalling(a1_3, true);

          fstream.close();

          await promiseRestartManager();

          AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1_4) {
            do_check_eq(a1_4, null);
            var dir = profileDir.clone();
            dir.append(do_get_expected_addon_name("addon1@tests.mozilla.org"));
            do_check_false(dir.exists());
            do_check_false(isExtensionInAddonsList(profileDir, "addon1@tests.mozilla.org"));

            do_execute_soon(do_test_finished);
          });
        }));
      }));
    }));
  });
}
