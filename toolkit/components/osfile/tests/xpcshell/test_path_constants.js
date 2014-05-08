/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Cu.import("resource://gre/modules/ctypes.jsm", this);
Cu.import("resource://testing-common/AppData.jsm", this);


function run_test() {
  run_next_test();
}

function compare_paths(ospath, key) {
  let file;
  try {
    file = Services.dirsvc.get(key, Components.interfaces.nsIFile);
  } catch(ex) {}

  if (file) {
    do_check_true(!!ospath);
    do_check_eq(ospath, file.path);
  } else {
    do_print("WARNING: " + key + " is not defined. Test may not be testing anything!");
    do_check_false(!!ospath);
  }
}

// Some path constants aren't set up until the profile is available. This
// test verifies that behavior.
add_task(function* test_before_after_profile() {
  do_check_null(OS.Constants.Path.profileDir);
  do_check_null(OS.Constants.Path.localProfileDir);
  do_check_null(OS.Constants.Path.userApplicationDataDir);

  do_get_profile();
  do_check_true(!!OS.Constants.Path.profileDir);
  do_check_true(!!OS.Constants.Path.localProfileDir);

  // UAppData is still null because the xpcshell profile doesn't set it up.
  // This test is mostly here to fail in case behavior of do_get_profile() ever
  // changes. We want to know if our assumptions no longer hold!
  do_check_null(OS.Constants.Path.userApplicationDataDir);

  yield makeFakeAppDir();
  do_check_true(!!OS.Constants.Path.userApplicationDataDir);

  // FUTURE: verify AppData too (bug 964291).
});

// Test simple paths
add_task(function* test_simple_paths() {
  do_check_true(!!OS.Constants.Path.tmpDir);
  compare_paths(OS.Constants.Path.tmpDir, "TmpD");

});

// Test presence of paths that only exist on Desktop platforms
add_task(function* test_desktop_paths() {
  if (OS.Constants.Sys.Name == "Android") {
    return;
  }
  do_check_true(!!OS.Constants.Path.desktopDir);
  do_check_true(!!OS.Constants.Path.homeDir);

  compare_paths(OS.Constants.Path.homeDir, "Home");
  compare_paths(OS.Constants.Path.desktopDir, "Desk");
  compare_paths(OS.Constants.Path.userApplicationDataDir, "UAppData");

  compare_paths(OS.Constants.Path.winAppDataDir, "AppData");
  compare_paths(OS.Constants.Path.winStartMenuProgsDir, "Progs");

  compare_paths(OS.Constants.Path.macUserLibDir, "ULibDir");
  compare_paths(OS.Constants.Path.macLocalApplicationsDir, "LocApp");
});

// Open libxul
add_task(function* test_libxul() {
  ctypes.open(OS.Constants.Path.libxul);
  do_print("Linked to libxul");
});
