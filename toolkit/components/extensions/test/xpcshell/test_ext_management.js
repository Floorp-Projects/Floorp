/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* test_management_schema() {
  function background() {
    browser.test.assertTrue(browser.management, "browser.management API exists");
    browser.test.notifyPass("management-schema");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["management"],
    },
    background: `(${background})()`,
  });
  yield extension.startup();
  yield extension.awaitFinish("management-schema");
  yield extension.unload();
});
