/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_downloads_api_namespace_and_permissions() {
  function backgroundScript() {
    browser.test.assertTrue(!!browser.downloads, "`downloads` API is present.");
    browser.test.assertTrue(
      !!browser.downloads.FilenameConflictAction,
      "`downloads.FilenameConflictAction` enum is present."
    );
    browser.test.assertTrue(
      !!browser.downloads.InterruptReason,
      "`downloads.InterruptReason` enum is present."
    );
    browser.test.assertTrue(
      !!browser.downloads.DangerType,
      "`downloads.DangerType` enum is present."
    );
    browser.test.assertTrue(
      !!browser.downloads.State,
      "`downloads.State` enum is present."
    );
    browser.test.notifyPass("downloads tests");
  }

  let extensionData = {
    background: backgroundScript,
    manifest: {
      permissions: ["downloads", "downloads.open"],
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  await extension.awaitFinish("downloads tests");
  await extension.unload();
});
