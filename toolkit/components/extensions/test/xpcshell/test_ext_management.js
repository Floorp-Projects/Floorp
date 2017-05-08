/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");

add_task(function* setup() {
  yield ExtensionTestUtils.startAddonManager();
});

add_task(function* test_management_schema() {
  async function background() {
    browser.test.assertTrue(browser.management, "browser.management API exists");
    let self = await browser.management.getSelf();
    browser.test.assertEq(browser.runtime.id, self.id, "got self");
    browser.test.notifyPass("management-schema");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["management"],
    },
    background: `(${background})()`,
    useAddonManager: "temporary",
  });
  yield extension.startup();
  yield extension.awaitFinish("management-schema");
  yield extension.unload();
});
