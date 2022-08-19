"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
XPCOMUtils.defineLazyServiceGetter(
  this,
  "gProxyService",
  "@mozilla.org/network/protocol-proxy-service;1",
  "nsIProtocolProxyService"
);
var { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

let pacServer;
const proxyPort = 4433;

add_setup(async function() {
  pacServer = new HttpServer();
  pacServer.registerPathHandler("/proxy.pac", function handler(
    metadata,
    response
  ) {
    let content = `function FindProxyForURL(url, host) { return "HTTPS localhost:${proxyPort}"; }`;
    response.setHeader("Content-Length", `${content.length}`);
    response.bodyOutputStream.write(content, content.length);
  });
  pacServer.start(-1);
});

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("network.proxy.type");
  Services.prefs.clearUserPref("network.proxy.autoconfig_url");
  Services.prefs.clearUserPref("network.proxy.reload_pac_delay");
});

async function getProxyInfo() {
  return new Promise((resolve, reject) => {
    let uri = Services.io.newURI("http://www.mozilla.org/");
    gProxyService.asyncResolve(uri, 0, {
      onProxyAvailable(_req, _uri, pi, _status) {
        resolve(pi);
      },
    });
  });
}

// Test if we can successfully get PAC when the PAC server is available.
add_task(async function testPAC() {
  // Configure PAC
  Services.prefs.setIntPref("network.proxy.type", 2);
  Services.prefs.setCharPref(
    "network.proxy.autoconfig_url",
    `http://localhost:${pacServer.identity.primaryPort}/proxy.pac`
  );

  let pi = await getProxyInfo();
  Assert.equal(pi.port, proxyPort, "Expected proxy port to be the same");
  Assert.equal(pi.type, "https", "Expected proxy type to be https");
});

// When PAC server is down, we should not use proxy at all.
add_task(async function testWhenPACServerDown() {
  Services.prefs.setIntPref("network.proxy.reload_pac_delay", 0);
  await new Promise(resolve => pacServer.stop(resolve));

  Services.obs.notifyObservers(null, "network:link-status-changed", "changed");

  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 3000));

  let pi = await getProxyInfo();
  Assert.equal(pi, null, "should have no proxy");
});
