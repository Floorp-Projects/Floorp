"use strict";

const server = createHttpServer({hosts: ["example.com"]});

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write("<!DOCTYPE html><html></html>");
});


add_task(async function test_userContextId_proxy_onRequest() {
  // This extension will succeed if it gets a request
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["proxy", "<all_urls>", "cookies"],
    },
    background() {
      browser.proxy.onRequest.addListener(async (details) => {
        browser.test.assertEq(details.cookieStoreId, "firefox-container-2", "cookieStoreId is set");
        browser.test.notifyPass("proxy.onRequest");
      }, {urls: ["<all_urls>"]});
    },
  });
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage("http://example.com/dummy", {userContextId: 2});
  await extension.awaitFinish("proxy.onRequest");
  await extension.unload();
  await contentPage.close();
});

add_task(async function test_userContextId_proxy_onRequest_nopermission() {
  // This extension will succeed if it gets a request
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["proxy", "<all_urls>"],
    },
    background() {
      browser.proxy.onRequest.addListener(async (details) => {
        browser.test.assertEq(details.cookieStoreId, undefined, "cookieStoreId not set, requires cookies permission");
        browser.test.notifyPass("proxy.onRequest");
      }, {urls: ["<all_urls>"]});
    },
  });
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage("http://example.com/dummy", {userContextId: 2});
  await extension.awaitFinish("proxy.onRequest");
  await extension.unload();
  await contentPage.close();
});
