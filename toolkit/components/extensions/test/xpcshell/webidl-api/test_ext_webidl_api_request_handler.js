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

// Verify ExtensionAPIRequestHandler handling API requests for
// an ext-*.js API module running in the main process
// (toolkit/components/extensions/parent/ext-alarms.js).
add_task(async function test_sw_api_request_handling_main_process_api() {
  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      background: {
        service_worker: "sw.js",
      },
      permissions: ["alarms"],
      applications: { gecko: { id: "test-bg-sw@mochi.test" } },
    },
    files: {
      "page.html": "<!DOCTYPE html><body></body>",
      "sw.js": async function() {
        browser.alarms.create("test-alarm", { when: Date.now() + 2000000 });
        const all = await browser.alarms.getAll();
        if (all.length === 1 && all[0].name === "test-alarm") {
          browser.test.succeed("Got the expected alarms");
        } else {
          browser.test.fail(
            `browser.alarms.create didn't create the expected alarm: ${JSON.stringify(
              all
            )}`
          );
        }

        browser.alarms.onAlarm.addListener(alarm => {
          if (alarm.name === "test-onAlarm") {
            browser.test.succeed("Got the expected onAlarm event");
          } else {
            browser.test.fail(`Got unexpected onAlarm event: ${alarm.name}`);
          }
          browser.test.sendMessage("test-completed");
        });

        browser.alarms.create("test-onAlarm", { when: Date.now() + 1000 });
      },
    },
  });

  await extension.startup();
  await extension.awaitMessage("test-completed");
  await extension.unload();
});

add_task(async function test_sw_api_request_bgsw_runtime_onMessage() {
  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      background: {
        service_worker: "sw.js",
      },
      permissions: [],
      applications: { gecko: { id: "test-bg-sw-on-message@mochi.test" } },
    },
    files: {
      "page.html": '<!DOCTYPE html><script src="page.js"></script>',
      "page.js": async function() {
        browser.test.onMessage.addListener(msg => {
          if (msg !== "extpage-send-message") {
            browser.test.fail(`Unexpected message received: ${msg}`);
            return;
          }
          browser.runtime.sendMessage("extpage-send-message");
        });
      },
      "sw.js": async function() {
        browser.runtime.onMessage.addListener(msg => {
          browser.test.sendMessage("bgsw-on-message", msg);
        });
        const extURL = browser.runtime.getURL("/");
        browser.test.sendMessage("ext-url", extURL);
      },
    },
  });

  await extension.startup();
  const extURL = await extension.awaitMessage("ext-url");
  equal(
    extURL,
    `moz-extension://${extension.uuid}/`,
    "Got the expected extension url"
  );

  const extPage = await ExtensionTestUtils.loadContentPage(
    `${extURL}/page.html`,
    { extension }
  );
  extension.sendMessage("extpage-send-message");

  const msg = await extension.awaitMessage("bgsw-on-message");
  equal(msg, "extpage-send-message", "Got the expected message");
  await extPage.close();
  await extension.unload();
});

add_task(async function test_sw_api_request_bgsw_runtime_sendMessage() {
  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      background: {
        service_worker: "sw.js",
      },
      permissions: [],
      applications: { gecko: { id: "test-bg-sw-sendMessage@mochi.test" } },
    },
    files: {
      "page.html": '<!DOCTYPE html><script src="page.js"></script>',
      "page.js": async function() {
        browser.runtime.onMessage.addListener(msg => {
          browser.test.sendMessage("extpage-on-message", msg);
        });

        browser.test.sendMessage("extpage-ready");
      },
      "sw.js": async function() {
        browser.test.onMessage.addListener(msg => {
          if (msg !== "bgsw-send-message") {
            browser.test.fail(`Unexpected message received: ${msg}`);
            return;
          }
          browser.runtime.sendMessage("bgsw-send-message");
        });
        const extURL = browser.runtime.getURL("/");
        browser.test.sendMessage("ext-url", extURL);
      },
    },
  });

  await extension.startup();
  const extURL = await extension.awaitMessage("ext-url");
  equal(
    extURL,
    `moz-extension://${extension.uuid}/`,
    "Got the expected extension url"
  );

  const extPage = await ExtensionTestUtils.loadContentPage(
    `${extURL}/page.html`,
    { extension }
  );
  await extension.awaitMessage("extpage-ready");
  extension.sendMessage("bgsw-send-message");

  const msg = await extension.awaitMessage("extpage-on-message");
  equal(msg, "bgsw-send-message", "Got the expected message");
  await extPage.close();
  await extension.unload();
});
