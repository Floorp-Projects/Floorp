"use strict";

const proxy = createHttpServer();

// accept proxy connections for mozilla.org
proxy.identity.add("http", "mozilla.org", 80);

proxy.registerPathHandler("/", (request, response) => {
  if (request.hasHeader("Proxy-Authorization")) {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write("ok");
  } else {
    response.setStatusLine(request.httpVersion, 407, "Proxy authentication required");
    response.setHeader("Proxy-Authenticate", 'Basic realm="foobar"', false);
    response.write("ok");
  }
});

add_task(async function test_webRequest_auth_proxy() {
  async function background() {
    browser.webRequest.onAuthRequired.addListener((details) => {
      browser.test.assertTrue(details.isProxy, "proxied request");
      browser.test.sendMessage("done", details.challenger);
      return {authCredentials: {username: "puser", password: "ppass"}};
    }, {urls: ["<all_urls>"]}, ["blocking"]);

    await browser.proxy.register("proxy.js");
    browser.test.sendMessage("pac-ready");
  }

  let handlingExt = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: [
        "proxy",
        "webRequest",
        "webRequestBlocking",
        "<all_urls>",
      ],
    },
    background,
    files: {
      "proxy.js": `
        function FindProxyForURL(url, host) {
          return "PROXY localhost:${proxy.identity.primaryPort}; DIRECT";
        }`,
    },
  });

  await handlingExt.startup();
  await handlingExt.awaitMessage("pac-ready");

  let contentPage = await ExtensionTestUtils.loadContentPage(`http://mozilla.org/`);

  let challenger = await handlingExt.awaitMessage("done");
  equal(challenger.host, "localhost", "proxy host");
  equal(challenger.port, proxy.identity.primaryPort, "proxy port");
  await contentPage.close();
  await handlingExt.unload();
}).only();
