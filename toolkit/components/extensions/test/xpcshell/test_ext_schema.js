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

add_task(async function test_warnings_as_errors() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: { unrecognized_property_that_should_be_treated_as_a_warning: 1 },
  });

  // Tests should be run with extensions.webextensions.warnings-as-errors=true
  // by default, and prevent extensions with manifest warnings from loading.
  await Assert.rejects(
    extension.startup(),
    /unrecognized_property_that_should_be_treated_as_a_warning/,
    "extension with invalid manifest should not load if warnings-as-errors=true"
  );
  // When ExtensionTestUtils.failOnSchemaWarnings(false) is called, startup is
  // expected to succeed, as shown by the next "testUnknownProperties" test.
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
    ExtensionTestUtils.failOnSchemaWarnings(false);
    await extension.startup();
    ExtensionTestUtils.failOnSchemaWarnings(true);
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
