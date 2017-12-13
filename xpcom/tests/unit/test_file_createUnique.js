/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/Services.jsm");

function run_test() {
  // Generate a leaf name that is 255 characters long.
  var longLeafName = new Array(256).join("T");

  // Generate the path for a file located in a directory with a long name.
  var tempFile = Services.dirsvc.get("TmpD", Ci.nsIFile);
  tempFile.append(longLeafName);
  tempFile.append("test.txt");

  try {
    tempFile.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o600);
    do_throw("Creating an item in a folder with a very long name should throw");
  } catch (e) {
    if (!(e instanceof Ci.nsIException &&
          e.result == Cr.NS_ERROR_FILE_UNRECOGNIZED_PATH)) {
      throw e;
    }
    // We expect the function not to crash but to raise this exception.
  }
}
