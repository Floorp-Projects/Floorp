"use strict";

add_task(async function test_unknown_permissions() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: [
        "activeTab",
        "fooUnknownPermission",
        "http://*/",
        "chrome://favicon/",
      ],
    },
  });

  let {messages} = await promiseConsoleOutput(
    () => extension.startup());

  const {WebExtensionPolicy} = Cu.import("resource://gre/modules/Extension.jsm", {});

  let policy = WebExtensionPolicy.getByID(extension.id);
  Assert.deepEqual(policy.permissions, ["activeTab", "http://*/*"]);

  ok(messages.some(message => /Error processing permissions\.1: Value "fooUnknownPermission" must/.test(message)),
     'Got expected error for "fooUnknownPermission"');

  ok(messages.some(message => /Error processing permissions\.3: Value "chrome:\/\/favicon\/" must/.test(message)),
     'Got expected error for "chrome://favicon/"');

  await extension.unload();
});
