"use strict";

const server = createHttpServer({ hosts: ["example.com"] });
server.registerPathHandler("/HELLO", (req, res) => {
  res.write("BYE");
});

add_task(async function request_from_extension_page() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["http://example.com/", "webRequest", "webRequestBlocking"],
    },
    files: {
      "tab.html": `<!DOCTYPE html><script src="tab.js"></script>`,
      "tab.js": async function() {
        browser.webRequest.onHeadersReceived.addListener(
          details => {
            let { responseHeaders } = details;
            responseHeaders.push({
              name: "X-Added-by-Test",
              value: "TheValue",
            });
            return { responseHeaders };
          },
          {
            urls: ["http://example.com/HELLO"],
          },
          ["blocking", "responseHeaders"]
        );

        // Ensure that listener is registered (workaround for bug 1300234).
        await browser.runtime.getPlatformInfo();

        let response = await fetch("http://example.com/HELLO");
        browser.test.assertEq(
          "TheValue",
          response.headers.get("X-added-by-test"),
          "expected response header from webRequest listener"
        );
        browser.test.assertEq(
          await response.text(),
          "BYE",
          "Expected response from server"
        );
        browser.test.sendMessage("done");
      },
    },
  });
  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    `moz-extension://${extension.uuid}/tab.html`,
    { extension }
  );
  await extension.awaitMessage("done");
  await contentPage.close();
  await extension.unload();
});
