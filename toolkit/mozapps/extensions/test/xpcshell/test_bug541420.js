/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  startupManager();

  installAllFiles([do_get_file("data/test_bug541420.xpi")], function() {

    restartManager();

    AddonManager.getAddonByID("bug541420@tests.mozilla.org", function(addon) {

      Assert.notEqual(addon, null);
      Assert.ok(addon.hasResource("binary"));
      let uri = addon.getResourceURI("binary");
      Assert.ok(uri instanceof AM_Ci.nsIFileURL);
      let file = uri.file;
      Assert.ok(file.exists());
      Assert.ok(file.isReadable());
      Assert.ok(file.isWritable());

      // We don't understand executable permissions on Windows since we don't
      // support NTFS permissions so we don't need to test there. OSX's isExecutable
      // only tests if the file is an application so it is better to just check the
      // raw permission bits
      if (!("nsIWindowsRegKey" in Components.interfaces))
        Assert.ok((file.permissions & 0o100) == 0o100);

      do_execute_soon(do_test_finished);
    });
  });
}
