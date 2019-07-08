"use strict";

const server = createHttpServer({ hosts: ["example.com"] });

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write("<!DOCTYPE html><html></html>");
});

add_task(async function test_userContextId_webrequest() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: [
        "webRequest",
        "webRequestBlocking",
        "<all_urls>",
        "cookies",
      ],
    },
    background() {
      browser.webRequest.onBeforeRequest.addListener(
        async details => {
          browser.test.assertEq(
            details.cookieStoreId,
            "firefox-container-2",
            "cookieStoreId is set"
          );
          browser.test.notifyPass("webRequest");
        },
        { urls: ["<all_urls>"] },
        ["blocking"]
      );
    },
  });
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy",
    { userContextId: 2 }
  );
  await extension.awaitFinish("webRequest");

  await extension.unload();
  await contentPage.close();
});

add_task(async function test_userContextId_webrequest_nopermission() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "<all_urls>"],
    },
    background() {
      browser.webRequest.onBeforeRequest.addListener(
        async details => {
          browser.test.assertEq(
            details.cookieStoreId,
            undefined,
            "cookieStoreId not set, requires cookies permission"
          );
          browser.test.notifyPass("webRequest");
        },
        { urls: ["<all_urls>"] },
        ["blocking"]
      );
    },
  });
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy",
    { userContextId: 2 }
  );
  await extension.awaitFinish("webRequest");

  await extension.unload();
  await contentPage.close();
});
