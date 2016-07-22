/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testIdle() {
  function background() {
    browser.idle.queryState(15).then(status => {
      browser.test.assertEq("active", status, "Expected status");
      browser.test.notifyPass("idle");
    },
    e => {
      browser.test.fail(`Error: ${e} :: ${e.stack}`);
      browser.test.notifyFail("idle");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,

    manifest: {
      permissions: ["idle"],
    },
  });

  yield extension.startup();

  yield extension.awaitFinish("idle");

  yield extension.unload();
});
