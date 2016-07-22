/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function backgroundScript() {
  browser.runtime.getPlatformInfo(info => {
    let validOSs = ["mac", "win", "android", "cros", "linux", "openbsd", "gonk"];
    let validArchs = ["arm", "x86-32", "x86-64"];

    browser.test.assertTrue(validOSs.indexOf(info.os) != -1, "OS is valid");
    browser.test.assertTrue(validArchs.indexOf(info.arch) != -1, "Architecture is valid");
    browser.test.notifyPass("runtime.getPlatformInfo");
  });
}

let extensionData = {
  background: backgroundScript,
};

add_task(function* test_contentscript() {
  let extension = ExtensionTestUtils.loadExtension(extensionData);
  yield extension.startup();
  yield extension.awaitFinish("runtime.getPlatformInfo");
  yield extension.unload();
});
