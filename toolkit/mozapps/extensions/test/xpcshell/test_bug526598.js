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
                                 callback_soon(function([a1, a2]) {

      Assert.notEqual(a1, null);
      Assert.ok(a1.hasResource("install.rdf"));
      let uri = a1.getResourceURI("install.rdf");
      Assert.ok(uri instanceof AM_Ci.nsIFileURL);
      let file = uri.file;
      Assert.ok(file.exists());
      Assert.ok(file.isReadable());
      Assert.ok(file.isWritable());

      Assert.notEqual(a2, null);
      Assert.ok(a2.hasResource("install.rdf"));
      uri = a2.getResourceURI("install.rdf");
      Assert.ok(uri instanceof AM_Ci.nsIFileURL);
      file = uri.file;
      Assert.ok(file.exists());
      Assert.ok(file.isReadable());
      Assert.ok(file.isWritable());

      a1.uninstall();
      a2.uninstall();

      restartManager();

      AddonManager.getAddonsByIDs(["bug526598_1@tests.mozilla.org",
                                   "bug526598_2@tests.mozilla.org"],
                                   function([newa1, newa2]) {
        Assert.equal(newa1, null);
        Assert.equal(newa2, null);

        executeSoon(do_test_finished);
      });
    }));
  });
}
