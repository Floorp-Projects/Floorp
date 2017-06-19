/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
Cu.import("resource://gre/modules/NetUtil.jsm");

// Check that reading non existant inner jars results in the right error

function run_test() {
  var file = do_get_file("data/test_bug597702.zip");
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  var outerJarBase = "jar:" + ios.newFileURI(file).spec + "!/";
  var goodSpec = "jar:" + outerJarBase + "inner.jar!/hello#!/ignore%20this%20part";
  var goodChannel = NetUtil.newChannel({uri: goodSpec, loadUsingSystemPrincipal: true});
  var instr = goodChannel.open2();

  ok(!!instr, "Should be able to open channel");
}
