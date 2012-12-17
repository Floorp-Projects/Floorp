/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

function run_test() {
  run_next_test();
}

add_test(function test_initialize_profileDir() {
  // Profile has not been set up yet, check that "profileDir" isn't either.
  do_check_false("profileDir" in OS.Constants.Path);
  do_check_false("localProfileDir" in OS.Constants.Path);

  // Set up profile.
  do_get_profile();

  // Now that profile has been set up, check that "profileDir" is set.
  do_check_true("profileDir" in OS.Constants.Path);
  do_check_true(!!OS.Constants.Path.profileDir);
  do_check_eq(OS.Constants.Path.profileDir,
              Services.dirsvc.get("ProfD", Components.interfaces.nsIFile).path);

  do_check_true("localProfileDir" in OS.Constants.Path);
  do_check_true(!!OS.Constants.Path.localProfileDir);
  do_check_eq(OS.Constants.Path.localProfileDir,
              Services.dirsvc.get("ProfLD", Components.interfaces.nsIFile).path);

  let promise = OS.File.makeDir(OS.Path.join(OS.Constants.Path.profileDir, "foobar"));
  promise.then(
    function onSuccess() {
      do_print("Directory creation succeeded");
      run_next_test();
    },

    function onFailure(reason) {
      do_fail(reason);
      run_next_test();
    }
  );
});
