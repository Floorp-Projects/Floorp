"use strict";

const server = createHttpServer({ hosts: ["example.com"] });

server.registerPathHandler("/dummy", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  response.write("<!DOCTYPE html><html></html>");
});

add_task(async function test_userContextId_proxy() {
  Services.prefs.setBoolPref("extensions.userContextIsolation.enabled", true);
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["proxy", "<all_urls>"],
    },
    background() {
      browser.proxy.onRequest.addListener(
        async details => {
          browser.test.assertEq(
            "firefox-container-2",
            details.cookieStoreId,
            "cookieStoreId is set"
          );
          browser.test.notifyPass("allowed");
        },
        { urls: ["http://example.com/dummy"] }
      );
    },
  });

  Services.prefs.setCharPref(
    "extensions.userContextIsolation.defaults.restricted",
    "[1]"
  );
  await extension.startup();

  let restrictedPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy",
    { userContextId: 1 }
  );

  let allowedPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy",
    {
      userContextId: 2,
    }
  );
  await extension.awaitFinish("allowed");

  await extension.unload();
  await restrictedPage.close();
  await allowedPage.close();

  Services.prefs.clearUserPref("extensions.userContextIsolation.enabled");
  Services.prefs.clearUserPref(
    "extensions.userContextIsolation.defaults.restricted"
  );
});
