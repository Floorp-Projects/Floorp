"use strict";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "authManager",
  "@mozilla.org/network/http-auth-manager;1",
  "nsIHttpAuthManager"
);

const proxy = createHttpServer();
const proxyToken = "this_is_my_pass";

// accept proxy connections for mozilla.org
proxy.identity.add("http", "mozilla.org", 80);

proxy.registerPathHandler("/", (request, response) => {
  if (request.hasHeader("Proxy-Authorization")) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/plain", false);
    response.write(request.getHeader("Proxy-Authorization"));
  } else {
    response.setStatusLine(
      request.httpVersion,
      407,
      "Proxy authentication required"
    );
    response.setHeader("Content-Type", "text/plain", false);
    response.setHeader("Proxy-Authenticate", "UnknownMeantToFail", false);
    response.write("auth required");
  }
});

function getExtension(background) {
  return ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["proxy", "webRequest", "webRequestBlocking", "<all_urls>"],
    },
    background: `(${background})(${proxy.identity.primaryPort}, "${proxyToken}")`,
  });
}

add_task(async function test_webRequest_auth_proxy() {
  function background(port, proxyToken) {
    browser.webRequest.onCompleted.addListener(
      details => {
        browser.test.log(`onCompleted ${JSON.stringify(details)}\n`);
        browser.test.assertEq(
          "localhost",
          details.proxyInfo.host,
          "proxy host"
        );
        browser.test.assertEq(port, details.proxyInfo.port, "proxy port");
        browser.test.assertEq("http", details.proxyInfo.type, "proxy type");
        browser.test.assertEq(
          "",
          details.proxyInfo.username,
          "proxy username not set"
        );
        browser.test.assertEq(
          proxyToken,
          details.proxyInfo.proxyAuthorizationHeader,
          "proxy authorization header"
        );
        browser.test.assertEq(
          proxyToken,
          details.proxyInfo.connectionIsolationKey,
          "proxy connection isolation"
        );

        browser.test.notifyPass("requestCompleted");
      },
      { urls: ["<all_urls>"] }
    );

    browser.webRequest.onAuthRequired.addListener(
      details => {
        // Using proxyAuthorizationHeader should prevent an auth request coming to us in the extension.
        browser.test.fail("onAuthRequired");
      },
      { urls: ["<all_urls>"] },
      ["blocking"]
    );

    // Handle the proxy request.
    browser.proxy.onRequest.addListener(
      details => {
        browser.test.log(`onRequest ${JSON.stringify(details)}`);
        return [
          {
            host: "localhost",
            port,
            type: "http",
            proxyAuthorizationHeader: proxyToken,
            connectionIsolationKey: proxyToken,
          },
        ];
      },
      { urls: ["<all_urls>"] },
      ["requestHeaders"]
    );
  }

  let extension = getExtension(background);

  await extension.startup();

  authManager.clearAll();

  let contentPage = await ExtensionTestUtils.loadContentPage(
    `http://mozilla.org/`
  );

  await extension.awaitFinish("requestCompleted");
  await contentPage.close();
  await extension.unload();
});
