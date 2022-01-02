"use strict";

// Test for Bug 1579911: Check that download requests created by the
// downloads.download API can be observed by extensions.
add_task(async function testDownload() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: [
        "webRequest",
        "webRequestBlocking",
        "downloads",
        "https://example.com/*",
      ],
    },
    background: async function() {
      browser.webRequest.onBeforeRequest.addListener(
        details => {
          browser.test.sendMessage("request_intercepted");
          return { cancel: true };
        },
        {
          urls: ["https://example.com/downloadtest"],
        },
        ["blocking"]
      );

      browser.downloads.onChanged.addListener(delta => {
        browser.test.assertEq(delta.state.current, "interrupted");
        browser.test.sendMessage("done");
      });

      await browser.downloads.download({
        url: "https://example.com/downloadtest",
        filename: "example.txt",
      });
    },
  });

  await extension.startup();
  await extension.awaitMessage("request_intercepted");
  await extension.awaitMessage("done");
  await extension.unload();
});
