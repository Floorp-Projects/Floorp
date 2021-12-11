"use strict";

const server = createHttpServer({ hosts: ["example.com"] });

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write("ok");
});

add_task(async function test_webRequest_viewsource() {
  function background(serverPort) {
    browser.proxy.onRequest.addListener(
      details => {
        if (details.url === `http://example.com:${serverPort}/dummy`) {
          browser.test.assertTrue(
            true,
            "viewsource protocol worked in proxy request"
          );
          browser.test.sendMessage("proxied");
        }
      },
      { urls: ["<all_urls>"] }
    );

    browser.webRequest.onBeforeRequest.addListener(
      details => {
        browser.test.assertEq(
          `http://example.com:${serverPort}/redirect`,
          details.url,
          "viewsource protocol worked in webRequest"
        );
        browser.test.sendMessage("viewed");
        return { redirectUrl: `http://example.com:${serverPort}/dummy` };
      },
      { urls: ["http://example.com/redirect"] },
      ["blocking"]
    );

    browser.webRequest.onBeforeRequest.addListener(
      details => {
        browser.test.assertEq(
          `http://example.com:${serverPort}/dummy`,
          details.url,
          "viewsource protocol worked in webRequest"
        );
        browser.test.sendMessage("redirected");
        return { cancel: true };
      },
      { urls: ["http://example.com/dummy"] },
      ["blocking"]
    );

    browser.webRequest.onCompleted.addListener(
      details => {
        // If cancel fails we get onCompleted.
        browser.test.fail("onCompleted received");
      },
      { urls: ["http://example.com/dummy"] }
    );

    browser.webRequest.onErrorOccurred.addListener(
      details => {
        browser.test.assertEq(
          details.error,
          "NS_ERROR_ABORT",
          "request cancelled"
        );
        browser.test.sendMessage("cancelled");
      },
      { urls: ["http://example.com/dummy"] }
    );
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["proxy", "webRequest", "webRequestBlocking", "<all_urls>"],
    },
    background: `(${background})(${server.identity.primaryPort})`,
  });
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `view-source:http://example.com:${server.identity.primaryPort}/redirect`
  );

  await Promise.all([
    extension.awaitMessage("proxied"),
    extension.awaitMessage("viewed"),
    extension.awaitMessage("redirected"),
    extension.awaitMessage("cancelled"),
  ]);

  await contentPage.close();
  await extension.unload();
});
