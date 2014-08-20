/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

let Path = OS.Path;
let profileDir;

do_register_cleanup(function() {
  Services.prefs.setBoolPref("toolkit.osfile.log", false);
});

function run_test() {
  run_next_test();
}

/**
 * Test OS.File.makeDir
 */

add_task(function init() {
  // Set up profile. We create the directory in the profile, because the profile
  // is removed after every test run.
  do_get_profile();
  profileDir = OS.Constants.Path.profileDir;
  Services.prefs.setBoolPref("toolkit.osfile.log", true);
});

/**
 * Basic use
 */

add_task(function* test_basic() {
  let dir = Path.join(profileDir, "directory");

  // Sanity checking for the test
  do_check_false((yield OS.File.exists(dir)));

  // Make a directory
  yield OS.File.makeDir(dir);

  //check if the directory exists
  yield OS.File.stat(dir);

  // Make a directory that already exists, this should succeed
  yield OS.File.makeDir(dir);

  // Make a directory with ignoreExisting
  yield OS.File.makeDir(dir, {ignoreExisting: true});

  // Make a directory with ignoreExisting false
  let exception = null;
  try {
    yield OS.File.makeDir(dir, {ignoreExisting: false});
  } catch (ex) {
    exception = ex;
  }

  do_check_true(!!exception);
  do_check_true(exception instanceof OS.File.Error);
  do_check_true(exception.becauseExists);
});

// Make a root directory that already exists
add_task(function* test_root() {
  if (OS.Constants.Win) {
    yield OS.File.makeDir("C:");
    yield OS.File.makeDir("C:\\");
  } else {
    yield OS.File.makeDir("/");
  }
});

/**
 * Creating subdirectories
 */
add_task(function test_option_from() {
  let dir = Path.join(profileDir, "a", "b", "c");

  // Sanity checking for the test
  do_check_false((yield OS.File.exists(dir)));

  // Make a directory
  yield OS.File.makeDir(dir, {from: profileDir});

  //check if the directory exists
  yield OS.File.stat(dir);

  // Make a directory that already exists, this should succeed
  yield OS.File.makeDir(dir);

  // Make a directory with ignoreExisting
  yield OS.File.makeDir(dir, {ignoreExisting: true});

  // Make a directory with ignoreExisting false
  let exception = null;
  try {
    yield OS.File.makeDir(dir, {ignoreExisting: false});
  } catch (ex) {
    exception = ex;
  }

  do_check_true(!!exception);
  do_check_true(exception instanceof OS.File.Error);
  do_check_true(exception.becauseExists);

  // Make a directory without |from| and fail
  let dir2 = Path.join(profileDir, "g", "h", "i");
  exception = null;
  try {
    yield OS.File.makeDir(dir2);
  } catch (ex) {
    exception = ex;
  }

  do_check_true(!!exception);
  do_check_true(exception instanceof OS.File.Error);
  do_check_true(exception.becauseNoSuchFile);

  // Test edge cases on paths

  let dir3 = Path.join(profileDir, "d", "", "e", "f");
  do_check_false((yield OS.File.exists(dir3)));
  yield OS.File.makeDir(dir3, {from: profileDir});
  do_check_true((yield OS.File.exists(dir3)));

  let dir4;
  if (OS.Constants.Win) {
    // Test that we can create a directory recursively even
    // if we have too many "\\".
    dir4 = profileDir + "\\\\g";
  } else {
    dir4 = profileDir + "////g";
  }
  do_check_false((yield OS.File.exists(dir4)));
  yield OS.File.makeDir(dir4, {from: profileDir});
  do_check_true((yield OS.File.exists(dir4)));
});
