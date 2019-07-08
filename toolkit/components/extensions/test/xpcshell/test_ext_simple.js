/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_simple() {
  let extensionData = {
    manifest: {
      name: "Simple extension test",
      version: "1.0",
      manifest_version: 2,
      description: "",
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  await extension.unload();
});

add_task(async function test_background() {
  function background() {
    browser.test.log("running background script");

    browser.test.onMessage.addListener((x, y) => {
      browser.test.assertEq(x, 10, "x is 10");
      browser.test.assertEq(y, 20, "y is 20");

      browser.test.notifyPass("background test passed");
    });

    browser.test.sendMessage("running", 1);
  }

  let extensionData = {
    background,
    manifest: {
      name: "Simple extension test",
      version: "1.0",
      manifest_version: 2,
      description: "",
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);

  let [, x] = await Promise.all([
    extension.startup(),
    extension.awaitMessage("running"),
  ]);
  equal(x, 1, "got correct value from extension");

  extension.sendMessage(10, 20);
  await extension.awaitFinish();
  await extension.unload();
});

add_task(async function test_extensionTypes() {
  let extensionData = {
    background: function() {
      browser.test.assertEq(
        typeof browser.extensionTypes,
        "object",
        "browser.extensionTypes exists"
      );
      browser.test.assertEq(
        typeof browser.extensionTypes.RunAt,
        "object",
        "browser.extensionTypes.RunAt exists"
      );
      browser.test.notifyPass("extentionTypes test passed");
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);

  await extension.startup();
  await extension.awaitFinish();
  await extension.unload();
});
