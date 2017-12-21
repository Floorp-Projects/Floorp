/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

registerCleanupFunction(function() {
  Services.prefs.setBoolPref("toolkit.osfile.log", false);
});

function run_test() {
  Services.prefs.setBoolPref("toolkit.osfile.log", true);
  run_next_test();
}

add_task(async function test_ignoreAbsent() {
  let absent_file_name = "test_osfile_front_absent.tmp";

  // Removing absent files should throw if "ignoreAbsent" is true.
  await Assert.rejects(OS.File.remove(absent_file_name, {ignoreAbsent: false}),
                       "OS.File.remove throws if there is no such file.");

  // Removing absent files should not throw if "ignoreAbsent" is true or not
  // defined.
  let exception = null;
  try {
    await OS.File.remove(absent_file_name, {ignoreAbsent: true});
    await OS.File.remove(absent_file_name);
  } catch (ex) {
    exception = ex;
  }
  Assert.ok(!exception, "OS.File.remove should not throw when not requested.");
});

add_task(async function test_ignoreAbsent_directory_missing() {
  let absent_file_name = OS.Path.join("absent_parent", "test.tmp");

  // Removing absent files should throw if "ignoreAbsent" is true.
  await Assert.rejects(OS.File.remove(absent_file_name, {ignoreAbsent: false}),
                       "OS.File.remove throws if there is no such file.");

  // Removing files from absent directories should not throw if "ignoreAbsent"
  // is true or not defined.
  let exception = null;
  try {
    await OS.File.remove(absent_file_name, {ignoreAbsent: true});
    await OS.File.remove(absent_file_name);
  } catch (ex) {
    exception = ex;
  }
  Assert.ok(!exception, "OS.File.remove should not throw when not requested.");
});
