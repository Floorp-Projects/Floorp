/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;

// Check that reading non existant inner jars results in the right error

function run_test() {
  var file = do_get_file("data/test_bug597702.zip");
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  var outerJarBase = "jar:" + ios.newFileURI(file).spec + "!/";
  var goodSpec = "jar:" + outerJarBase + "inner.jar!/hello";
  var badSpec = "jar:" + outerJarBase + "jar_that_isnt_in_the.jar!/hello";
  var goodChannel = ios.newChannel(goodSpec, null, null);
  var badChannel = ios.newChannel(badSpec, null, null);

  try {
    instr = goodChannel.open();
  } catch (e) {
    do_throw("Failed to open file in inner jar");
  }

  try {
    instr = badChannel.open();
    do_throw("Failed to report that file doesn't exist");
  } catch (e) {
    do_check_true(e.name == "NS_ERROR_FILE_NOT_FOUND");
  }
}

