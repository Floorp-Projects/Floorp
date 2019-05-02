"use strict";

const server = createHttpServer({hosts: ["example.com"]});

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write("<!DOCTYPE html><html></html>");
});

add_task(async function test_incognito_webrequest_access() {
  Services.prefs.setBoolPref("extensions.allowPrivateBrowsingByDefault", false);

  let pb_extension = ExtensionTestUtils.loadExtension({
    incognitoOverride: "spanning",
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "<all_urls>"],
    },
    background() {
      browser.webRequest.onBeforeRequest.addListener(async (details) => {
        browser.test.assertTrue(details.incognito, "incognito flag is set");
      }, {urls: ["<all_urls>"], incognito: true}, ["blocking"]);

      browser.webRequest.onBeforeRequest.addListener(async (details) => {
        browser.test.assertFalse(details.incognito, "incognito flag is not set");
        browser.test.notifyPass("webRequest.spanning");
      }, {urls: ["<all_urls>"], incognito: false}, ["blocking"]);
    },
  });
  await pb_extension.startup();

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "<all_urls>"],
    },
    background() {
      browser.webRequest.onBeforeRequest.addListener(async (details) => {
        browser.test.assertFalse(details.incognito, "incognito flag is not set");
        browser.test.notifyPass("webRequest");
      }, {urls: ["<all_urls>"]}, ["blocking"]);
    },
  });
  // Load non-incognito extension to check that private requests are invisible to it.
  await extension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage("http://example.com/dummy", {privateBrowsing: true});
  await contentPage.close();

  contentPage = await ExtensionTestUtils.loadContentPage("http://example.com/dummy");
  await extension.awaitFinish("webRequest");
  await pb_extension.awaitFinish("webRequest.spanning");
  await contentPage.close();

  await pb_extension.unload();
  await extension.unload();

  Services.prefs.clearUserPref("extensions.allowPrivateBrowsingByDefault");
});
