/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_runtime_sendMessage() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      background: {
        service_worker: "sw.js",
      },
      browser_specific_settings: {
        gecko: { id: "@test-messaging" },
      },
    },
    files: {
      "sw.js": async function () {
        browser.test.onMessage.addListener(msg => {
          browser.test.assertEq("send", msg, "expected correct message");
          browser.runtime.sendMessage("hello-from-sw");
        });

        browser.test.sendMessage("background-ready");
      },
      "page.html": '<!DOCTYPE html><script src="page.js"></script>',
      "page.js"() {
        browser.runtime.onMessage.addListener((msg, sender) => {
          browser.test.assertEq(
            "hello-from-sw",
            msg,
            "expected message from service worker"
          );

          const { contextId, ...otherProps } = sender;
          browser.test.assertTrue(!!contextId, "expected a truthy contextId");
          browser.test.assertDeepEq(
            {
              envType: "addon_child",
              id: "@test-messaging",
              origin: self.origin,
              url: browser.runtime.getURL("sw.js"),
            },
            otherProps,
            "expected correct sender props"
          );

          browser.test.sendMessage("page-done");
        });

        browser.test.sendMessage("page-ready");
      },
    },
  });

  await extension.startup();
  await extension.awaitMessage("background-ready");

  const page = await ExtensionTestUtils.loadContentPage(
    `moz-extension://${extension.uuid}/page.html`,
    { extension }
  );
  await extension.awaitMessage("page-ready");

  extension.sendMessage("send");
  await extension.awaitMessage("page-done");

  await page.close();
  await extension.unload();
});

add_task(async function test_runtime_connect() {
  const extension = ExtensionTestUtils.loadExtension({
    manifest: {
      background: {
        service_worker: "sw.js",
      },
      browser_specific_settings: {
        gecko: { id: "@test-messaging" },
      },
    },
    files: {
      "sw.js": async function () {
        browser.test.onMessage.addListener(msg => {
          browser.test.assertEq("connect", msg, "expected correct message");
          browser.runtime.connect();
        });

        browser.test.sendMessage("background-ready");
      },
      "page.html": '<!DOCTYPE html><script src="page.js"></script>',
      "page.js"() {
        browser.runtime.onConnect.addListener(port => {
          const { contextId, ...otherProps } = port.sender;
          browser.test.assertTrue(!!contextId, "expected a truthy contextId");
          browser.test.assertDeepEq(
            {
              envType: "addon_child",
              id: "@test-messaging",
              origin: self.origin,
              url: browser.runtime.getURL("sw.js"),
            },
            otherProps,
            "expected correct sender props"
          );

          browser.test.sendMessage("page-done");
        });

        browser.test.sendMessage("page-ready");
      },
    },
  });

  await extension.startup();
  await extension.awaitMessage("background-ready");

  const page = await ExtensionTestUtils.loadContentPage(
    `moz-extension://${extension.uuid}/page.html`,
    { extension }
  );
  await extension.awaitMessage("page-ready");

  extension.sendMessage("connect");
  await extension.awaitMessage("page-done");

  await page.close();
  await extension.unload();
});
