"use strict";

ChromeUtils.import("resource://gre/modules/ExtensionUtils.jsm");
ChromeUtils.defineModuleGetter(this, "HttpServer",
                               "resource://testing-common/httpd.js");

const {
  createAppInfo,
  promiseShutdownManager,
  promiseStartupManager,
} = AddonTestUtils;

AddonTestUtils.init(this);

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "42");

// We cannot use createHttpServer because it also messes with proxies.  We want
// httpChannel to pick up the prefs we set and use those to proxy to our server.
// If this were to fail, we would get an error about making a request out to
// the network.
const proxy = new HttpServer();
proxy.start(-1);
proxy.registerPathHandler("/fubar", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write("ok");
});
registerCleanupFunction(() => {
  return new Promise(resolve => {
    proxy.stop(resolve);
  });
});

add_task(async function test_proxy_settings() {
  async function background(host, port) {
    browser.webRequest.onBeforeRequest.addListener(details => {
      browser.test.assertEq(host, details.proxyInfo.host, "proxy host matched");
      browser.test.assertEq(port, details.proxyInfo.port, "proxy port matched");
    }, {urls: ["http://example.com/*"]});
    browser.webRequest.onCompleted.addListener(details => {
      browser.test.notifyPass("proxytest");
    }, {urls: ["http://example.com/*"]});
    browser.webRequest.onErrorOccurred.addListener(details => {
      browser.test.notifyFail("proxytest");
    }, {urls: ["http://example.com/*"]});

    // Wait for the settings before testing a request.
    await browser.proxy.settings.set({value: {
      proxyType: "manual",
      http: `${host}:${port}`,
    }});
    browser.test.sendMessage("ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {gecko: {id: "proxy.settings@mochi.test"}},
      permissions: [
        "proxy",
        "webRequest",
        "<all_urls>",
      ],
    },
    useAddonManager: "temporary",
    background: `(${background})("${proxy.identity.primaryHost}", ${proxy.identity.primaryPort})`,
  });

  await promiseStartupManager();
  await extension.startup();
  await extension.awaitMessage("ready");
  equal(Services.prefs.getStringPref("network.proxy.http"), proxy.identity.primaryHost, "proxy address is set");
  equal(Services.prefs.getIntPref("network.proxy.http_port"), proxy.identity.primaryPort, "proxy port is set");
  let ok = extension.awaitFinish("proxytest");
  let contentPage = await ExtensionTestUtils.loadContentPage("http://example.com/fubar");
  await ok;

  await contentPage.close();
  await extension.unload();
  await promiseShutdownManager();
});
