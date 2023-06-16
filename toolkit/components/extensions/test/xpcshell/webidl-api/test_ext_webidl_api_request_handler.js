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
      browser_specific_settings: { gecko: { id: "test-bg-sw@mochi.test" } },
    },
    files: {
      "page.html": "<!DOCTYPE html><body></body>",
      "sw.js": async function () {
        browser.test.onMessage.addListener(async msg => {
          browser.test.succeed("call to test.succeed");
          browser.test.assertTrue(true, "call to test.assertTrue");
          browser.test.assertFalse(false, "call to test.assertFalse");
          // Smoke test assertEq (more complete coverage of the behavior expected
          // by the test API will be introduced in test_ext_test.html as part of
          // Bug 1723785).
          const errorObject = new Error("fake_error_message");
          browser.test.assertEq(
            errorObject,
            errorObject,
            "call to test.assertEq"
          );

          // Smoke test for assertThrows/assertRejects.
          const errorMatchingTestCases = [
            ["expected error instance", errorObject],
            ["expected error message string", "fake_error_message"],
            ["expected regexp", /fake_error/],
            ["matching function", error => errorObject === error],
            ["matching Constructor", Error],
          ];

          browser.test.log("run assertThrows smoke tests");

          const throwFn = () => {
            throw errorObject;
          };
          for (const [msg, expected] of errorMatchingTestCases) {
            browser.test.assertThrows(
              throwFn,
              expected,
              `call to assertThrow with ${msg}`
            );
          }

          browser.test.log("run assertRejects smoke tests");

          const rejectedPromise = Promise.reject(errorObject);
          for (const [msg, expected] of errorMatchingTestCases) {
            await browser.test.assertRejects(
              rejectedPromise,
              expected,
              `call to assertRejects with ${msg}`
            );
          }

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
      browser_specific_settings: { gecko: { id: "test-bg-sw@mochi.test" } },
    },
    files: {
      "page.html": "<!DOCTYPE html><body></body>",
      "sw.js": async function () {
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
      browser_specific_settings: {
        gecko: { id: "test-bg-sw-on-message@mochi.test" },
      },
    },
    files: {
      "page.html": '<!DOCTYPE html><script src="page.js"></script>',
      "page.js": async function () {
        browser.test.onMessage.addListener(msg => {
          if (msg !== "extpage-send-message") {
            browser.test.fail(`Unexpected message received: ${msg}`);
            return;
          }
          browser.runtime.sendMessage("extpage-send-message");
        });
      },
      "sw.js": async function () {
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
      browser_specific_settings: {
        gecko: { id: "test-bg-sw-sendMessage@mochi.test" },
      },
    },
    files: {
      "page.html": '<!DOCTYPE html><script src="page.js"></script>',
      "page.js": async function () {
        browser.runtime.onMessage.addListener(msg => {
          browser.test.sendMessage("extpage-on-message", msg);
        });

        browser.test.sendMessage("extpage-ready");
      },
      "sw.js": async function () {
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

// Verify ExtensionAPIRequestHandler handling API requests that
// returns a runtinme.Port API object.
add_task(async function test_sw_api_request_bgsw_connnect_runtime_port() {
  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      background: {
        service_worker: "sw.js",
      },
      permissions: [],
      browser_specific_settings: { gecko: { id: "test-bg-sw@mochi.test" } },
    },
    files: {
      "page.html": '<!DOCTYPE html><script src="page.js"></script>',
      "page.js": async function () {
        browser.runtime.onConnect.addListener(port => {
          browser.test.sendMessage("page-got-port-from-sw");
          port.postMessage("page-to-sw");
        });
        browser.test.sendMessage("page-waiting-port");
      },
      "sw.js": async function () {
        browser.test.onMessage.addListener(msg => {
          if (msg !== "connect-port") {
            return;
          }
          const port = browser.runtime.connect();
          if (!port) {
            browser.test.fail("Got an undefined port");
          }
          port.onMessage.addListener((msg, portArgument) => {
            browser.test.assertTrue(
              port === portArgument,
              "Got the expected runtime.Port instance"
            );
            browser.test.sendMessage("test-done", msg);
          });
          browser.test.sendMessage("sw-waiting-port-message");
        });

        const portWithError = browser.runtime.connect();
        portWithError.onDisconnect.addListener(() => {
          const portError = portWithError.error;
          browser.test.sendMessage("port-error", {
            isError: portError instanceof Error,
            message: portError?.message,
          });
        });

        const extURL = browser.runtime.getURL("/");
        browser.test.sendMessage("ext-url", extURL);
        browser.test.sendMessage("ext-id", browser.runtime.id);
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

  const extId = await extension.awaitMessage("ext-id");
  equal(extId, extension.id, "Got the expected extension id");

  const lastError = await extension.awaitMessage("port-error");
  Assert.deepEqual(
    lastError,
    {
      isError: true,
      message: "Could not establish connection. Receiving end does not exist.",
    },
    "Got the expected lastError value"
  );

  const extPage = await ExtensionTestUtils.loadContentPage(
    `${extURL}/page.html`,
    { extension }
  );
  await extension.awaitMessage("page-waiting-port");

  info("bgsw connect port");
  extension.sendMessage("connect-port");
  await extension.awaitMessage("sw-waiting-port-message");
  info("bgsw waiting port message");
  await extension.awaitMessage("page-got-port-from-sw");
  info("page got port from sw, wait to receive event");
  const msg = await extension.awaitMessage("test-done");
  equal(msg, "page-to-sw", "Got the expected message");
  await extPage.close();
  await extension.unload();
});

// Verify ExtensionAPIRequestHandler handling API events that should
// get a runtinme.Port API object as an event argument.
add_task(async function test_sw_api_request_bgsw_runtime_onConnect() {
  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      background: {
        service_worker: "sw.js",
      },
      permissions: [],
      browser_specific_settings: {
        gecko: { id: "test-bg-sw-onConnect@mochi.test" },
      },
    },
    files: {
      "page.html": '<!DOCTYPE html><script src="page.js"></script>',
      "page.js": async function () {
        browser.test.onMessage.addListener(msg => {
          if (msg !== "connect-port") {
            return;
          }
          const port = browser.runtime.connect();
          port.onMessage.addListener(msg => {
            browser.test.sendMessage("test-done", msg);
          });
          browser.test.sendMessage("page-waiting-port-message");
        });
      },
      "sw.js": async function () {
        try {
          const extURL = browser.runtime.getURL("/");
          browser.test.sendMessage("ext-url", extURL);

          browser.runtime.onConnect.addListener(port => {
            browser.test.sendMessage("bgsw-got-port-from-page");
            port.postMessage("sw-to-page");
          });
          browser.test.sendMessage("bgsw-waiting-port");
        } catch (err) {
          browser.test.fail(`Error on runtime.onConnect: ${err}`);
        }
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
  await extension.awaitMessage("bgsw-waiting-port");

  const extPage = await ExtensionTestUtils.loadContentPage(
    `${extURL}/page.html`,
    { extension }
  );
  info("ext page connect port");
  extension.sendMessage("connect-port");

  await extension.awaitMessage("page-waiting-port-message");
  info("page waiting port message");
  await extension.awaitMessage("bgsw-got-port-from-page");
  info("bgsw got port from page, page wait to receive event");
  const msg = await extension.awaitMessage("test-done");
  equal(msg, "sw-to-page", "Got the expected message");
  await extPage.close();
  await extension.unload();
});

add_task(async function test_sw_runtime_lastError() {
  const extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "temporary",
    manifest: {
      background: {
        service_worker: "sw.js",
      },
      browser_specific_settings: { gecko: { id: "test-bg-sw@mochi.test" } },
    },
    files: {
      "page.html": "<!DOCTYPE html><body></body>",
      "sw.js": async function () {
        browser.runtime.sendMessage(() => {
          const lastError = browser.runtime.lastError;
          if (!(lastError instanceof Error)) {
            browser.test.fail(
              `lastError isn't an Error instance: ${lastError}`
            );
          }
          browser.test.sendMessage("test-lastError-completed");
        });
      },
    },
  });

  await extension.startup();
  await extension.awaitMessage("test-lastError-completed");
  await extension.unload();
});
