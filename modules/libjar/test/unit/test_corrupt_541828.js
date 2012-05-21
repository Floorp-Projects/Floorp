/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Check that files can be read from after closing zipreader
function run_test() {
  const Cc = Components.classes;
  const Ci = Components.interfaces;

  // the build script have created the zip we can test on in the current dir.
  var file = do_get_file("data/test_corrupt2.zip");

  var zipreader = Cc["@mozilla.org/libjar/zip-reader;1"].
                  createInstance(Ci.nsIZipReader);
  var failed = false;
  try {
    zipreader.open(file);
    // corrupt files should trigger an exception
  } catch (ex) {
    failed = true;
  }
  do_check_true(failed);
}

