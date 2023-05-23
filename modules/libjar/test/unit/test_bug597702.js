/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

// Check that reading non existant inner jars results in the right error

function run_test() {
  var file = do_get_file("data/test_bug597702.zip");
  var outerJarBase = "jar:" + Services.io.newFileURI(file).spec + "!/";
  var goodSpec = "jar:" + outerJarBase + "inner.jar!/hello";
  var badSpec = "jar:" + outerJarBase + "jar_that_isnt_in_the.jar!/hello";
  var goodChannel = NetUtil.newChannel({
    uri: goodSpec,
    loadUsingSystemPrincipal: true,
  });
  var badChannel = NetUtil.newChannel({
    uri: badSpec,
    loadUsingSystemPrincipal: true,
  });

  try {
    goodChannel.open();
  } catch (e) {
    do_throw("Failed to open file in inner jar");
  }

  try {
    badChannel.open();
    do_throw("Failed to report that file doesn't exist");
  } catch (e) {
    Assert.ok(e.name == "NS_ERROR_FILE_NOT_FOUND");
  }
}
