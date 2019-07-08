/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_connect_without_listener() {
  function background() {
    let port = browser.runtime.connect();
    port.onDisconnect.addListener(() => {
      browser.test.assertEq(
        "Could not establish connection. Receiving end does not exist.",
        port.error && port.error.message
      );
      browser.test.notifyPass("port.onDisconnect was called");
    });
  }
  let extensionData = {
    background,
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  await extension.awaitFinish("port.onDisconnect was called");

  await extension.unload();
});
