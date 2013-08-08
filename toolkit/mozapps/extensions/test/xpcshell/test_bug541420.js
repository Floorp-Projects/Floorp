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

      do_check_neq(addon, null);
      do_check_true(addon.hasResource("binary"));
      let uri = addon.getResourceURI("binary");
      do_check_true(uri instanceof AM_Ci.nsIFileURL);
      let file = uri.file;
      do_check_true(file.exists());
      do_check_true(file.isReadable());
      do_check_true(file.isWritable());

      // We don't understand executable permissions on Windows since we don't
      // support NTFS permissions so we don't need to test there. OSX's isExecutable
      // only tests if the file is an application so it is better to just check the
      // raw permission bits
      if (!("nsIWindowsRegKey" in Components.interfaces))
        do_check_true((file.permissions & 0100) == 0100);

      do_execute_soon(do_test_finished);
    });
  });
}
