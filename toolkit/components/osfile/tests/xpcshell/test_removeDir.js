/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource://gre/modules/Task.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

do_register_cleanup(function() {
  Services.prefs.setBoolPref("toolkit.osfile.log", false);
});

function run_test() {
  Services.prefs.setBoolPref("toolkit.osfile.log", true);

  run_next_test();
}

add_task(function() {
  // Set up profile. We create the directory in the profile, because the profile
  // is removed after every test run.
  do_get_profile();

  let file = OS.Path.join(OS.Constants.Path.profileDir, "file");
  let dir = OS.Path.join(OS.Constants.Path.profileDir, "directory");
  let file1 = OS.Path.join(dir, "file1");
  let file2 = OS.Path.join(dir, "file2");
  let subDir = OS.Path.join(dir, "subdir");
  let fileInSubDir = OS.Path.join(subDir, "file");

  // Sanity checking for the test
  do_check_false((yield OS.File.exists(dir)));

  // Remove non-existent directory
  let exception = null;
  try {
    yield OS.File.removeDir(dir);
  } catch (ex) {
    exception = ex;
  }

  do_check_true(!!exception);
  do_check_true(exception instanceof OS.File.Error);

  // Remove non-existent directory with ignoreAbsent
  yield OS.File.removeDir(dir, {ignoreAbsent: true});

  // Remove file
  yield OS.File.writeAtomic(file, "content", { tmpPath: file + ".tmp" });
  exception = null;
  try {
    yield OS.File.removeDir(file);
  } catch (ex) {
    exception = ex;
  }

  do_check_true(!!exception);
  do_check_true(exception instanceof OS.File.Error);

  // Remove empty directory
  yield OS.File.makeDir(dir);
  yield OS.File.removeDir(dir);
  do_check_false((yield OS.File.exists(dir)));

  // Remove directory that contains one file
  yield OS.File.makeDir(dir);
  yield OS.File.writeAtomic(file1, "content", { tmpPath: file1 + ".tmp" });
  //yield OS.File.open(file1, {create:true});
  yield OS.File.removeDir(dir)
  do_check_false((yield OS.File.exists(dir)));

  // Remove directory that contains multiple files
  yield OS.File.makeDir(dir);
  yield OS.File.writeAtomic(file1, "content", { tmpPath: file1 + ".tmp" });
  yield OS.File.writeAtomic(file2, "content", { tmpPath: file2 + ".tmp" });
  yield OS.File.removeDir(dir)
  do_check_false((yield OS.File.exists(dir)));

  // Remove directory that contains a file and a directory
  yield OS.File.makeDir(dir);
  yield OS.File.writeAtomic(file1, "content", { tmpPath: file1 + ".tmp" });
  yield OS.File.makeDir(subDir);
  yield OS.File.writeAtomic(fileInSubDir, "content", { tmpPath: fileInSubDir + ".tmp" });
  yield OS.File.removeDir(dir);
  do_check_false((yield OS.File.exists(dir)));
});
