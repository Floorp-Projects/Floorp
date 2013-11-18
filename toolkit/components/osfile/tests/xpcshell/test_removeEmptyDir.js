/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

do_register_cleanup(function() {
  Services.prefs.setBoolPref("toolkit.osfile.log", false);
});

function run_test() {
  Services.prefs.setBoolPref("toolkit.osfile.log", true);

  run_next_test();
}

/**
 * Test OS.File.removeEmptyDir
 */
add_task(function() {
  // Set up profile. We create the directory in the profile, because the profile
  // is removed after every test run.
  do_get_profile();

  let dir = OS.Path.join(OS.Constants.Path.profileDir, "directory");

  // Sanity checking for the test
  do_check_false((yield OS.File.exists(dir)));

  // Remove non-existent directory
  yield OS.File.removeEmptyDir(dir);

  // Remove non-existent directory with ignoreAbsent
  yield OS.File.removeEmptyDir(dir, {ignoreAbsent: true});

  // Remove non-existent directory with ignoreAbsent false
  let exception = null;
  try {
    yield OS.File.removeEmptyDir(dir, {ignoreAbsent: false});
  } catch (ex) {
    exception = ex;
  }

  do_check_true(!!exception);
  do_check_true(exception instanceof OS.File.Error);
  do_check_true(exception.becauseNoSuchFile);

  // Remove empty directory
  yield OS.File.makeDir(dir);
  yield OS.File.removeEmptyDir(dir);
  do_check_false((yield OS.File.exists(dir)));
});
