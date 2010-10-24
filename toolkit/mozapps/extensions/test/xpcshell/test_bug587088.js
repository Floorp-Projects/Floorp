/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that trying to upgrade or uninstall an extension that has a file locked
// will roll back the upgrade

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

function check_addon(aAddon) {
  do_check_neq(aAddon, null);
  do_check_eq(aAddon.version, "1.0");
  do_check_true(isExtensionInAddonsList(profileDir, aAddon.id));

  do_check_true(aAddon.hasResource("testfile"));
  do_check_true(aAddon.hasResource("testfile1"));
  do_check_false(aAddon.hasResource("testfile2"));
}

function run_test_1() {
  installAllFiles([do_get_addon("test_bug587088_1")], function() {
    restartManager();

    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
      check_addon(a1);

      // Lock either install.rdf for unpacked add-ons or the xpi for packed add-ons.
      let uri = a1.getResourceURI("install.rdf");
      if (uri.schemeIs("jar"))
        uri = a1.getResourceURI();

      let fstream = AM_Cc["@mozilla.org/network/file-input-stream;1"].
                    createInstance(AM_Ci.nsIFileInputStream);
      fstream.init(uri.QueryInterface(AM_Ci.nsIFileURL).file, -1, 0, 0);

      installAllFiles([do_get_addon("test_bug587088_2")], function() {
        restartManager();
        fstream.close();

        AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
          check_addon(a1);

          a1.uninstall();
          restartManager();

          run_test_2();
        });
      });
    });
  });
}

// Test that a failed uninstall gets rolled back
function run_test_2() {
  installAllFiles([do_get_addon("test_bug587088_1")], function() {
    restartManager();

    AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
      check_addon(a1);

      // Lock either install.rdf for unpacked add-ons or the xpi for packed add-ons.
      let uri = a1.getResourceURI("install.rdf");
      if (uri.schemeIs("jar"))
        uri = a1.getResourceURI();

      let fstream = AM_Cc["@mozilla.org/network/file-input-stream;1"].
                    createInstance(AM_Ci.nsIFileInputStream);
      fstream.init(uri.QueryInterface(AM_Ci.nsIFileURL).file, -1, 0, 0);

      a1.uninstall();

      restartManager();

      fstream.close();

      AddonManager.getAddonByID("addon1@tests.mozilla.org", function(a1) {
        check_addon(a1);

        do_test_finished();
      });
    });
  });
}
