/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies the functionality of getResourceURI
// There are two cases - with a filename it returns an nsIFileURL to the filename
// and with no parameters, it returns an nsIFileURL to the root of the addon

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  startupManager();

  installAllFiles([do_get_file("data/unsigned.xpi")], function() {

    restartManager();

    AddonManager.getAddonByID("unsigned-xpi@tests.mozilla.org",
                                 function(a1) {

      do_check_neq(a1, null);
      do_check_true(a1.hasResource("install.rdf"));
      let uri = a1.getResourceURI("install.rdf");
      do_check_true(uri instanceof AM_Ci.nsIFileURL);

      let uri2 = a1.getResourceURI();
      do_check_true(uri2 instanceof AM_Ci.nsIFileURL);

      let addonDir = gProfD.clone();
      addonDir.append("extensions");
      addonDir.append("unsigned-xpi@tests.mozilla.org");

      do_check_eq(uri2.file.path, addonDir.path);

      a1.uninstall();

      restartManager();

      AddonManager.getAddonByID("unsigned-xpi@tests.mozilla.org",
                                   function(newa1) {
        do_check_eq(newa1, null);

        do_test_finished();
      });
    });
  });
}
