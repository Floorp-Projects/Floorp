"use strict";

const server = createHttpServer({hosts: ["example.com"]});

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write("<!DOCTYPE html><html></html>");
});

add_task(async function test_incognito_webrequest_access() {
  // This extension will fail if it gets a request
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "<all_urls>"],
    },
    incognitoOverride: "not_allowed",
    background() {
      browser.webRequest.onBeforeRequest.addListener(async (details) => {
        browser.test.fail("webrequest received incognito request");
      }, {urls: ["<all_urls>"]}, ["blocking"]);
    },
  });
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage("http://example.com/dummy", {privateBrowsing: true});

  await extension.unload();
  await contentPage.close();
});
