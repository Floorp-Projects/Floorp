/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

// Check that reading non existant inner jars results in the right error

add_task(async function() {
  var file = do_get_file("data/test_bug597702.zip");
  var outerJarBase = "jar:" + Services.io.newFileURI(file).spec + "!/";
  var goodSpec =
    "jar:" + outerJarBase + "inner.jar!/hello#!/ignore%20this%20part";
  var goodChannel = NetUtil.newChannel({
    uri: goodSpec,
    loadUsingSystemPrincipal: true,
  });
  var instr = goodChannel.open();

  ok(!!instr, "Should be able to open channel");
});

add_task(async function() {
  var file = do_get_file("data/test_bug597702.zip");
  var outerJarBase = "jar:" + Services.io.newFileURI(file).spec + "!/";
  var goodSpec =
    "jar:" + outerJarBase + "inner.jar!/hello?ignore%20this%20part!/";
  var goodChannel = NetUtil.newChannel({
    uri: goodSpec,
    loadUsingSystemPrincipal: true,
  });
  var instr = goodChannel.open();

  ok(!!instr, "Should be able to open channel");
});

add_task(async function() {
  var file = do_get_file("data/test_bug597702.zip");
  var outerJarBase = "jar:" + Services.io.newFileURI(file).spec + "!/";
  var goodSpec = "jar:" + outerJarBase + "inner.jar!/hello?ignore#this!/part";
  var goodChannel = NetUtil.newChannel({
    uri: goodSpec,
    loadUsingSystemPrincipal: true,
  });
  var instr = goodChannel.open();

  ok(!!instr, "Should be able to open channel");
});
