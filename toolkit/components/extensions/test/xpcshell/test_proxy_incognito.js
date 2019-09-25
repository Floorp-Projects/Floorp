"use strict";

/* eslint no-unused-vars: ["error", {"args": "none", "varsIgnorePattern": "^(FindProxyForURL)$"}] */

const server = createHttpServer({ hosts: ["example.com"] });

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write("<!DOCTYPE html><html></html>");
});

add_task(async function test_incognito_proxy_onRequest_access() {
  // No specific support exists in the proxy api for this test,
  // rather it depends on functionality existing in ChannelWrapper
  // that prevents notification of private channels if the
  // extension does not have permission.
  Services.prefs.setBoolPref("extensions.allowPrivateBrowsingByDefault", false);

  // This extension will fail if it gets a private request.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["proxy", "<all_urls>"],
    },
    async background() {
      browser.proxy.onRequest.addListener(
        async details => {
          browser.test.assertFalse(
            details.incognito,
            "incognito flag is not set"
          );
          browser.test.notifyPass("proxy.onRequest");
        },
        { urls: ["<all_urls>"], types: ["main_frame"] }
      );

      // Actual call arguments do not matter here.
      await browser.test.assertRejects(
        browser.proxy.settings.set({
          value: {
            proxyType: "none",
          },
        }),
        /proxy.settings requires private browsing permission/,
        "proxy.settings requires private browsing permission."
      );

      browser.test.sendMessage("ready");
    },
  });
  await extension.startup();
  await extension.awaitMessage("ready");

  let pextension = ExtensionTestUtils.loadExtension({
    incognitoOverride: "spanning",
    manifest: {
      permissions: ["proxy", "<all_urls>"],
    },
    background() {
      browser.proxy.onRequest.addListener(
        async details => {
          browser.test.assertTrue(
            details.incognito,
            "incognito flag is set with filter"
          );
          browser.test.sendMessage("proxy.onRequest.private");
        },
        { urls: ["<all_urls>"], types: ["main_frame"], incognito: true }
      );

      browser.proxy.onRequest.addListener(
        async details => {
          browser.test.assertFalse(
            details.incognito,
            "incognito flag is not set with filter"
          );
          browser.test.notifyPass("proxy.onRequest.spanning");
        },
        { urls: ["<all_urls>"], types: ["main_frame"], incognito: false }
      );
    },
  });
  await pextension.startup();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy",
    { privateBrowsing: true }
  );
  await pextension.awaitMessage("proxy.onRequest.private");
  await contentPage.close();

  contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy"
  );
  await extension.awaitFinish("proxy.onRequest");
  await pextension.awaitFinish("proxy.onRequest.spanning");
  await contentPage.close();

  await pextension.unload();
  await extension.unload();

  Services.prefs.clearUserPref("extensions.allowPrivateBrowsingByDefault");
});
