"use strict";

AddonTestUtils.init(this);

add_task(async function testEmptySchema() {
  function background() {
    browser.test.assertEq(
      undefined,
      browser.manifest,
      "browser.manifest is not defined"
    );
    browser.test.assertTrue(
      !!browser.storage,
      "browser.storage should be defined"
    );
    browser.test.assertEq(
      undefined,
      browser.contextMenus,
      "browser.contextMenus should not be defined"
    );
    browser.test.notifyPass("schema");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["storage"],
    },
  });

  await extension.startup();
  await extension.awaitFinish("schema");
  await extension.unload();
});

add_task(async function testUnknownProperties() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["unknownPermission"],

      unknown_property: {},
    },

    background() {},
  });

  let { messages } = await promiseConsoleOutput(async () => {
    await extension.startup();
  });

  AddonTestUtils.checkMessages(messages, {
    expected: [
      { message: /processing permissions\.0: Value "unknownPermission"/ },
      {
        message: /processing unknown_property: An unexpected property was found in the WebExtension manifest/,
      },
    ],
  });

  await extension.unload();
});
