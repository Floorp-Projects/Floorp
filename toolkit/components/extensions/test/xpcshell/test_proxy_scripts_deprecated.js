"use strict";

AddonTestUtils.init(this);

add_task(async function test_proxy_deprecation_messages() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["proxy"],
    },
    background() {
      async function callProxyAPIs() {
        browser.proxy.onProxyError.addListener(() => {});
        browser.proxy.onError.addListener(() => {});
        // Not expecting deprecation warning for this one:
        browser.proxy.onRequest.addListener(() => {}, {urls: ["http://x/*"]});
        await browser.proxy.register("proxy.js");
        await browser.proxy.unregister();
        await browser.proxy.registerProxyScript("proxy.js");
        await browser.proxy.unregister();
      }
      browser.test.onMessage.addListener(async () => {
        try {
          await callProxyAPIs();
        } catch (e) {
          browser.test.fail(e);
        }
        browser.test.sendMessage("done");
      });
    },
    files: {
      "proxy.js": "// Dummy script.",
    },
  });

  await extension.startup();

  let {messages} = await promiseConsoleOutput(async () => {
    extension.sendMessage("start-test");
    await extension.awaitMessage("done");
  });
  await extension.unload();
  AddonTestUtils.checkMessages(messages, {
    expected: [
      {message: /proxy\.onProxyError has been deprecated and will be removed in Firefox 71\. Use proxy.onError instead/},
      {message: /proxy\.register has been deprecated and will be removed in Firefox 71/},
      {message: /proxy\.unregister has been deprecated and will be removed in Firefox 71/},
      {message: /proxy\.registerProxyScript has been deprecated and will be removed in Firefox 71/},
      {message: /proxy\.unregister has been deprecated and will be removed in Firefox 71/},
    ],
  }, true);
});
