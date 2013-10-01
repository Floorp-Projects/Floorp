/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/osfile.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

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
    do_check_false(!!ospath);
  }
}

add_test(function() {
  do_check_true(!!OS.Constants.Path.tmpDir);
  do_check_eq(OS.Constants.Path.tmpDir, Services.dirsvc.get("TmpD", Components.interfaces.nsIFile).path);

  do_check_true(!!OS.Constants.Path.homeDir);
  do_check_eq(OS.Constants.Path.homeDir, Services.dirsvc.get("Home", Components.interfaces.nsIFile).path);

  do_check_true(!!OS.Constants.Path.desktopDir);
  do_check_eq(OS.Constants.Path.desktopDir, Services.dirsvc.get("Desk", Components.interfaces.nsIFile).path);

  compare_paths(OS.Constants.Path.winAppDataDir, "AppData");
  compare_paths(OS.Constants.Path.winStartMenuProgsDir, "Progs");

  compare_paths(OS.Constants.Path.macUserLibDir, "ULibDir");
  compare_paths(OS.Constants.Path.macLocalApplicationsDir, "LocApp");

  run_next_test();
});
