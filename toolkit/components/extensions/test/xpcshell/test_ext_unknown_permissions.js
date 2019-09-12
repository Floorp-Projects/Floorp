"use strict";

// This test expects and checks warnings for unknown permissions.
ExtensionTestUtils.failOnSchemaWarnings(false);

add_task(async function test_unknown_permissions() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: [
        "activeTab",
        "fooUnknownPermission",
        "http://*/",
        "chrome://favicon/",
      ],
      optional_permissions: ["chrome://favicon/", "https://example.com/"],
    },
  });

  let { messages } = await promiseConsoleOutput(() => extension.startup());

  const { WebExtensionPolicy } = Cu.getGlobalForObject(
    ChromeUtils.import("resource://gre/modules/Extension.jsm", {})
  );

  let policy = WebExtensionPolicy.getByID(extension.id);
  Assert.deepEqual(Array.from(policy.permissions).sort(), [
    "activeTab",
    "http://*/*",
  ]);

  Assert.deepEqual(extension.extension.manifest.optional_permissions, [
    "https://example.com/",
  ]);

  ok(
    messages.some(message =>
      /Error processing permissions\.1: Value "fooUnknownPermission" must/.test(
        message
      )
    ),
    'Got expected error for "fooUnknownPermission"'
  );

  ok(
    messages.some(message =>
      /Error processing permissions\.3: Value "chrome:\/\/favicon\/" must/.test(
        message
      )
    ),
    'Got expected error for "chrome://favicon/"'
  );

  ok(
    messages.some(message =>
      /Error processing optional_permissions\.0: Value "chrome:\/\/favicon\/" must/.test(
        message
      )
    ),
    "Got expected error from optional_permissions"
  );

  await extension.unload();
});
