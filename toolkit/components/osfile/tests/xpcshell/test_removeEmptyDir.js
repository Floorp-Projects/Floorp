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
add_task(async function() {
  // Set up profile. We create the directory in the profile, because the profile
  // is removed after every test run.
  do_get_profile();

  let dir = OS.Path.join(OS.Constants.Path.profileDir, "directory");

  // Sanity checking for the test
  Assert.equal(false, (await OS.File.exists(dir)));

  // Remove non-existent directory
  await OS.File.removeEmptyDir(dir);

  // Remove non-existent directory with ignoreAbsent
  await OS.File.removeEmptyDir(dir, {ignoreAbsent: true});

  // Remove non-existent directory with ignoreAbsent false
  let exception = null;
  try {
    await OS.File.removeEmptyDir(dir, {ignoreAbsent: false});
  } catch (ex) {
    exception = ex;
  }

  Assert.ok(!!exception);
  Assert.ok(exception instanceof OS.File.Error);
  Assert.ok(exception.becauseNoSuchFile);

  // Remove empty directory
  await OS.File.makeDir(dir);
  await OS.File.removeEmptyDir(dir);
  Assert.equal(false, (await OS.File.exists(dir)));
});
