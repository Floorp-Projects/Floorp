/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
  // Ensure that the profile-after-change message has been notified,
  // so that ServiceWokerRegistrar is going to be initialized.
  Services.obs.notifyObservers(
    null,
    "profile-after-change",
    "force-serviceworkerrestart-init"
  );
});

// Verify ExtensionAPIRequestHandler handling API requests for
// an ext-*.js API module running in the local process
// (toolkit/components/extensions/child/ext-test.js).
add_task(async function test_sw_api_request_handling_local_process_api() {
  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      background: {
        service_worker: "sw.js",
      },
      applications: { gecko: { id: "test-bg-sw@mochi.test" } },
    },
    files: {
      "page.html": "<!DOCTYPE html><body></body>",
      "sw.js": async function() {
        browser.test.onMessage.addListener(msg => {
          browser.test.succeed("call to test.succeed");
          browser.test.assertTrue(true, "call to test.assertTrue");
          browser.test.assertFalse(false, "call to test.assertFalse");
          browser.test.notifyPass("test-completed");
        });
        browser.test.sendMessage("bgsw-ready");
      },
    },
  });

  await extension.startup();
  await extension.awaitMessage("bgsw-ready");
  extension.sendMessage("test-message-ok");
  await extension.awaitFinish();
  await extension.unload();
});
