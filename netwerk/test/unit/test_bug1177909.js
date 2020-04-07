"use strict";

const { MockRegistrar } = ChromeUtils.import(
  "resource://testing-common/MockRegistrar.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gProxyService",
  "@mozilla.org/network/protocol-proxy-service;1",
  "nsIProtocolProxyService"
);

XPCOMUtils.defineLazyGetter(this, "systemSettings", function() {
  return {
    QueryInterface: ChromeUtils.generateQI(["nsISystemProxySettings"]),

    mainThreadOnly: true,
    PACURI: null,

    getProxyForURI(aSpec, aScheme, aHost, aPort) {
      if (aPort != -1) {
        return "SOCKS5 http://localhost:9050";
      }
      if (aScheme == "http" || aScheme == "ftp") {
        return "PROXY http://localhost:8080";
      }
      if (aScheme == "https") {
        return "HTTPS https://localhost:8080";
      }
      return "DIRECT";
    },
  };
});

let gMockProxy = MockRegistrar.register(
  "@mozilla.org/system-proxy-settings;1",
  systemSettings
);

registerCleanupFunction(() => {
  MockRegistrar.unregister(gMockProxy);
});

function makeChannel(uri) {
  return NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  });
}

async function TestProxyType(chan, flags) {
  const prefs = Cc["@mozilla.org/preferences-service;1"].getService(
    Ci.nsIPrefBranch
  );
  prefs.setIntPref(
    "network.proxy.type",
    Ci.nsIProtocolProxyService.PROXYCONFIG_SYSTEM
  );

  return new Promise((resolve, reject) => {
    gProxyService.asyncResolve(chan, flags, {
      onProxyAvailable(req, uri, pi, status) {
        resolve(pi);
      },
    });
  });
}

async function TestProxyTypeByURI(uri) {
  return TestProxyType(makeChannel(uri), 0);
}

add_task(async function testHttpProxy() {
  let pi = await TestProxyTypeByURI("http://www.mozilla.org/");
  equal(pi.host, "localhost", "Expected proxy host to be localhost");
  equal(pi.port, 8080, "Expected proxy port to be 8080");
  equal(pi.type, "http", "Expected proxy type to be http");
});

add_task(async function testHttpsProxy() {
  let pi = await TestProxyTypeByURI("https://www.mozilla.org/");
  equal(pi.host, "localhost", "Expected proxy host to be localhost");
  equal(pi.port, 8080, "Expected proxy port to be 8080");
  equal(pi.type, "https", "Expected proxy type to be https");
});

if (Services.prefs.getBoolPref("network.ftp.enabled")) {
  add_task(async function testFtpProxy() {
    let pi = await TestProxyTypeByURI("ftp://ftp.mozilla.org/");
    equal(pi.host, "localhost", "Expected proxy host to be localhost");
    equal(pi.port, 8080, "Expected proxy port to be 8080");
    equal(pi.type, "http", "Expected proxy type to be http");
  });
}

add_task(async function testSocksProxy() {
  let pi = await TestProxyTypeByURI("http://www.mozilla.org:1234/");
  equal(pi.host, "localhost", "Expected proxy host to be localhost");
  equal(pi.port, 9050, "Expected proxy port to be 8080");
  equal(pi.type, "socks", "Expected proxy type to be http");
});

add_task(async function testDirectProxy() {
  // Do what |WebSocketChannel::AsyncOpen| do, but do not prefer https proxy.
  let proxyURI = Cc["@mozilla.org/network/standard-url-mutator;1"]
    .createInstance(Ci.nsIURIMutator)
    .setSpec("wss://ws.mozilla.org/")
    .finalize();
  let uri = proxyURI
    .mutate()
    .setScheme("https")
    .finalize();

  let ioService = Cc["@mozilla.org/network/io-service;1"].getService(
    Ci.nsIIOService
  );
  let chan = ioService.newChannelFromURIWithProxyFlags(
    uri,
    proxyURI,
    0,
    null,
    Services.scriptSecurityManager.getSystemPrincipal(),
    null,
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
    Ci.nsIContentPolicy.TYPE_OTHER
  );

  let pi = await TestProxyType(chan, 0);
  equal(pi, null, "Expected proxy host to be null");
});

add_task(async function testWebSocketProxy() {
  // Do what |WebSocketChannel::AsyncOpen| do
  let proxyURI = Cc["@mozilla.org/network/standard-url-mutator;1"]
    .createInstance(Ci.nsIURIMutator)
    .setSpec("wss://ws.mozilla.org/")
    .finalize();
  let uri = proxyURI
    .mutate()
    .setScheme("https")
    .finalize();

  let proxyFlags =
    Ci.nsIProtocolProxyService.RESOLVE_PREFER_SOCKS_PROXY |
    Ci.nsIProtocolProxyService.RESOLVE_PREFER_HTTPS_PROXY |
    Ci.nsIProtocolProxyService.RESOLVE_ALWAYS_TUNNEL;

  let ioService = Cc["@mozilla.org/network/io-service;1"].getService(
    Ci.nsIIOService
  );
  let chan = ioService.newChannelFromURIWithProxyFlags(
    uri,
    proxyURI,
    proxyFlags,
    null,
    Services.scriptSecurityManager.getSystemPrincipal(),
    null,
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
    Ci.nsIContentPolicy.TYPE_OTHER
  );

  let pi = await TestProxyType(chan, proxyFlags);
  equal(pi.host, "localhost", "Expected proxy host to be localhost");
  equal(pi.port, 8080, "Expected proxy port to be 8080");
  equal(pi.type, "https", "Expected proxy type to be https");
});

add_task(async function testPreferHttpsProxy() {
  let uri = Cc["@mozilla.org/network/standard-url-mutator;1"]
    .createInstance(Ci.nsIURIMutator)
    .setSpec("http://mozilla.org/")
    .finalize();
  let proxyFlags = Ci.nsIProtocolProxyService.RESOLVE_PREFER_HTTPS_PROXY;

  let ioService = Cc["@mozilla.org/network/io-service;1"].getService(
    Ci.nsIIOService
  );
  let chan = ioService.newChannelFromURIWithProxyFlags(
    uri,
    null,
    proxyFlags,
    null,
    Services.scriptSecurityManager.getSystemPrincipal(),
    null,
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
    Ci.nsIContentPolicy.TYPE_OTHER
  );

  let pi = await TestProxyType(chan, proxyFlags);
  equal(pi.host, "localhost", "Expected proxy host to be localhost");
  equal(pi.port, 8080, "Expected proxy port to be 8080");
  equal(pi.type, "https", "Expected proxy type to be https");
});
