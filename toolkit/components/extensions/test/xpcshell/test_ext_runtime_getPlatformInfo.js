/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function backgroundScript() {
  browser.runtime.getPlatformInfo(info => {
    let validOSs = ["mac", "win", "android", "cros", "linux", "openbsd"];
    let validArchs = ["arm", "x86-32", "x86-64", "aarch64"];

    browser.test.assertTrue(validOSs.includes(info.os), "OS is valid");
    browser.test.assertTrue(
      validArchs.includes(info.arch),
      "Architecture is valid"
    );
    browser.test.notifyPass("runtime.getPlatformInfo");
  });
}

let extensionData = {
  background: backgroundScript,
};

add_task(async function() {
  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  await extension.awaitFinish("runtime.getPlatformInfo");
  await extension.unload();
});
