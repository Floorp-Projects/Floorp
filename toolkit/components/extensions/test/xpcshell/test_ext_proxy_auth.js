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
  async function background(port) {
    browser.webRequest.onBeforeRequest.addListener(details => {
      browser.test.log(`details ${JSON.stringify(details)}\n`);
      browser.test.assertEq("localhost", details.proxyInfo.host, "proxy host");
      browser.test.assertEq(port, details.proxyInfo.port, "proxy port");
      browser.test.assertEq("http", details.proxyInfo.type, "proxy type");
      browser.test.assertEq("", details.proxyInfo.username, "proxy username not set");
    }, {urls: ["<all_urls>"]});
    browser.webRequest.onAuthRequired.addListener(details => {
      browser.test.assertTrue(details.isProxy, "proxied request");
      browser.test.assertEq("localhost", details.proxyInfo.host, "proxy host");
      browser.test.assertEq(port, details.proxyInfo.port, "proxy port");
      browser.test.assertEq("http", details.proxyInfo.type, "proxy type");
      browser.test.assertEq("localhost", details.challenger.host, "proxy host");
      browser.test.assertEq(port, details.challenger.port, "proxy port");
      return {authCredentials: {username: "puser", password: "ppass"}};
    }, {urls: ["<all_urls>"]}, ["blocking"]);
    browser.webRequest.onCompleted.addListener(details => {
      browser.test.log(`details ${JSON.stringify(details)}\n`);
      browser.test.assertEq("localhost", details.proxyInfo.host, "proxy host");
      browser.test.assertEq(port, details.proxyInfo.port, "proxy port");
      browser.test.assertEq("http", details.proxyInfo.type, "proxy type");
      browser.test.assertEq("", details.proxyInfo.username, "proxy username not set by onAuthRequired");
      browser.test.assertEq(undefined, details.proxyInfo.password, "no proxy password");
      browser.test.sendMessage("done");
    }, {urls: ["<all_urls>"]});

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
    background: `(${background})(${proxy.identity.primaryPort})`,
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

  await handlingExt.awaitMessage("done");
  await contentPage.close();
  await handlingExt.unload();
}).only();
