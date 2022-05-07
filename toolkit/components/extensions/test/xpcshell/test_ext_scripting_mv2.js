"use strict";

add_task(async function test_scripting_enabled_in_mv2() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      manifest_version: 2,
      permissions: ["scripting"],
    },
    background() {
      browser.test.assertEq(
        "object",
        typeof browser.scripting,
        "expected scripting namespace to be defined"
      );

      browser.test.sendMessage("background-done");
    },
  });

  await extension.startup();
  await extension.awaitMessage("background-done");
  await extension.unload();
});
