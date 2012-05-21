/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  startupManager();

  installAllFiles([do_get_file("data/test_bug526598_1.xpi"),
                   do_get_file("data/test_bug526598_2.xpi")], function() {

    restartManager();

    AddonManager.getAddonsByIDs(["bug526598_1@tests.mozilla.org",
                                 "bug526598_2@tests.mozilla.org"],
                                 function([a1, a2]) {

      do_check_neq(a1, null);
      do_check_true(a1.hasResource("install.rdf"));
      let uri = a1.getResourceURI("install.rdf");
      do_check_true(uri instanceof AM_Ci.nsIFileURL);
      let file = uri.file;
      do_check_true(file.exists());
      do_check_true(file.isReadable());
      do_check_true(file.isWritable());

      do_check_neq(a2, null);
      do_check_true(a2.hasResource("install.rdf"));
      uri = a2.getResourceURI("install.rdf");
      do_check_true(uri instanceof AM_Ci.nsIFileURL);
      file = uri.file;
      do_check_true(file.exists());
      do_check_true(file.isReadable());
      do_check_true(file.isWritable());

      a1.uninstall();
      a2.uninstall();

      restartManager();

      AddonManager.getAddonsByIDs(["bug526598_1@tests.mozilla.org",
                                   "bug526598_2@tests.mozilla.org"],
                                   function([newa1, newa2]) {
        do_check_eq(newa1, null);
        do_check_eq(newa2, null);

        do_test_finished();
      });
    });
  });
}
