/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_sendMessage_without_listener() {
  async function background() {
    await browser.test.assertRejects(
      browser.runtime.sendMessage("msg"),
      "Could not establish connection. Receiving end does not exist.",
      "Correct error when there are no receivers from background"
    );

    browser.test.sendMessage("sendMessage-error-bg");
  }
  let extensionData = {
    background,
    files: {
      "page.html": `<!doctype><meta charset=utf-8><script src="page.js"></script>`,
      async "page.js"() {
        await browser.test.assertRejects(
          browser.runtime.sendMessage("msg"),
          "Could not establish connection. Receiving end does not exist.",
          "Correct error when there are no receivers from extension page"
        );

        browser.test.notifyPass("sendMessage-error-page");
      },
    },
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();
  await extension.awaitMessage("sendMessage-error-bg");

  let url = `moz-extension://${extension.uuid}/page.html`;
  let page = await ExtensionTestUtils.loadContentPage(url, { extension });
  await extension.awaitFinish("sendMessage-error-page");
  await page.close();

  await extension.unload();
});

add_task(async function test_chrome_sendMessage_without_listener() {
  function background() {
    /* globals chrome */
    browser.test.assertEq(
      null,
      chrome.runtime.lastError,
      "no lastError before call"
    );
    let retval = chrome.runtime.sendMessage("msg");
    browser.test.assertEq(
      null,
      chrome.runtime.lastError,
      "no lastError after call"
    );
    browser.test.assertEq(
      undefined,
      retval,
      "return value of chrome.runtime.sendMessage without callback"
    );

    let isAsyncCall = false;
    retval = chrome.runtime.sendMessage("msg", reply => {
      browser.test.assertEq(undefined, reply, "no reply");
      browser.test.assertTrue(
        isAsyncCall,
        "chrome.runtime.sendMessage's callback must be called asynchronously"
      );
      browser.test.assertEq(
        undefined,
        retval,
        "return value of chrome.runtime.sendMessage with callback"
      );
      browser.test.assertEq(
        "Could not establish connection. Receiving end does not exist.",
        chrome.runtime.lastError.message
      );
      browser.test.notifyPass("finished chrome.runtime.sendMessage");
    });
    isAsyncCall = true;
  }
  let extensionData = {
    background,
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);
  await extension.startup();

  await extension.awaitFinish("finished chrome.runtime.sendMessage");

  await extension.unload();
});
