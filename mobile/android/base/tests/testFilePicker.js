// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");

function ok(passed, text) {
  do_report_result(passed, text, Components.stack.caller, false);
}

function is(lhs, rhs, text) {
  do_report_result(lhs === rhs, text, Components.stack.caller, false);
}

add_test(function filepicker_open() {
  let chromeWin = Services.wm.getMostRecentWindow("navigator:browser");

  do_test_pending();

  let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  fp.appendFilter("Martian files", "*.martian");
  fp.appendFilters(Ci.nsIFilePicker.filterAll);
  fp.filterIndex = 0;

  let fpCallback = function(result) {
    if (result == Ci.nsIFilePicker.returnOK || result == Ci.nsIFilePicker.returnReplace) {
      do_print("File: " + fp.file.path);
      is(fp.file.path, "/mnt/sdcard/my-favorite-martian.png", "Retrieve the right martian file!");

      let files = fp.files;
      while (files.hasMoreElements()) {
        let file = files.getNext().QueryInterface(Ci.nsIFile);
        do_print("File: " + file.path);
        is(file.path, "/mnt/sdcard/my-favorite-martian.png", "Retrieve the right martian file from array!");
      }

      do_print("DOMFile: " + fp.domfile.mozFullPath);
      is(fp.domfile.mozFullPath, "/mnt/sdcard/my-favorite-martian.png", "Retrieve the right martian domfile!");

      let domfiles = fp.domfiles;
      while (domfiles.hasMoreElements()) {
        let domfile = domfiles.getNext();
        do_print("DOMFile: " + domfile.mozFullPath);
        is(domfile.mozFullPath, "/mnt/sdcard/my-favorite-martian.png", "Retrieve the right martian file from domfile array!");
      }

      do_test_finished();

      run_next_test();
    }
  };

  try {
    fp.init(chromeWin, "Open", Ci.nsIFilePicker.modeOpen);
  } catch(ex) {
    ok(false, "Android should support FilePicker.modeOpen: " + ex);
  }
  fp.open(fpCallback);
});

add_test(function filepicker_save() {
  let failed = false;
  let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  try {
    fp.init(null, "Save", Ci.nsIFilePicker.modeSave);
  } catch(ex) {
    failed = true;
  }
  ok(failed, "Android does not support FilePicker.modeSave");

  run_next_test();
});

run_next_test();

