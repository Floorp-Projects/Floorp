/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

var Path = OS.Path;
var profileDir;

do_register_cleanup(function() {
  Services.prefs.setBoolPref("toolkit.osfile.log", false);
});

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

add_task(async function test_basic() {
  let dir = Path.join(profileDir, "directory");

  // Sanity checking for the test
  Assert.equal(false, (await OS.File.exists(dir)));

  // Make a directory
  await OS.File.makeDir(dir);

  // check if the directory exists
  await OS.File.stat(dir);

  // Make a directory that already exists, this should succeed
  await OS.File.makeDir(dir);

  // Make a directory with ignoreExisting
  await OS.File.makeDir(dir, {ignoreExisting: true});

  // Make a directory with ignoreExisting false
  let exception = null;
  try {
    await OS.File.makeDir(dir, {ignoreExisting: false});
  } catch (ex) {
    exception = ex;
  }

  Assert.ok(!!exception);
  Assert.ok(exception instanceof OS.File.Error);
  Assert.ok(exception.becauseExists);
});

// Make a root directory that already exists
add_task(async function test_root() {
  if (OS.Constants.Win) {
    await OS.File.makeDir("C:");
    await OS.File.makeDir("C:\\");
  } else {
    await OS.File.makeDir("/");
  }
});

/**
 * Creating subdirectories
 */
add_task(async function test_option_from() {
  let dir = Path.join(profileDir, "a", "b", "c");

  // Sanity checking for the test
  Assert.equal(false, (await OS.File.exists(dir)));

  // Make a directory
  await OS.File.makeDir(dir, {from: profileDir});

  // check if the directory exists
  await OS.File.stat(dir);

  // Make a directory that already exists, this should succeed
  await OS.File.makeDir(dir);

  // Make a directory with ignoreExisting
  await OS.File.makeDir(dir, {ignoreExisting: true});

  // Make a directory with ignoreExisting false
  let exception = null;
  try {
    await OS.File.makeDir(dir, {ignoreExisting: false});
  } catch (ex) {
    exception = ex;
  }

  Assert.ok(!!exception);
  Assert.ok(exception instanceof OS.File.Error);
  Assert.ok(exception.becauseExists);

  // Make a directory without |from| and fail
  let dir2 = Path.join(profileDir, "g", "h", "i");
  exception = null;
  try {
    await OS.File.makeDir(dir2);
  } catch (ex) {
    exception = ex;
  }

  Assert.ok(!!exception);
  Assert.ok(exception instanceof OS.File.Error);
  Assert.ok(exception.becauseNoSuchFile);

  // Test edge cases on paths

  let dir3 = Path.join(profileDir, "d", "", "e", "f");
  Assert.equal(false, (await OS.File.exists(dir3)));
  await OS.File.makeDir(dir3, {from: profileDir});
  Assert.ok((await OS.File.exists(dir3)));

  let dir4;
  if (OS.Constants.Win) {
    // Test that we can create a directory recursively even
    // if we have too many "\\".
    dir4 = profileDir + "\\\\g";
  } else {
    dir4 = profileDir + "////g";
  }
  Assert.equal(false, (await OS.File.exists(dir4)));
  await OS.File.makeDir(dir4, {from: profileDir});
  Assert.ok((await OS.File.exists(dir4)));
});
