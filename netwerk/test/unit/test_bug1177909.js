"use strict";

const { MockRegistrar } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistrar.sys.mjs"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gProxyService",
  "@mozilla.org/network/protocol-proxy-service;1",
  "nsIProtocolProxyService"
);

ChromeUtils.defineLazyGetter(this, "systemSettings", function () {
  return {
    QueryInterface: ChromeUtils.generateQI(["nsISystemProxySettings"]),

    mainThreadOnly: true,
    PACURI: null,

    getProxyForURI(aSpec, aScheme, aHost, aPort) {
      if (aPort != -1) {
        return "SOCKS5 http://localhost:9050";
      }
      if (aScheme == "http") {
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
  Services.prefs.setIntPref(
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
  let uri = proxyURI.mutate().setScheme("https").finalize();

  let chan = Services.io.newChannelFromURIWithProxyFlags(
    uri,
    proxyURI,
    0,
    null,
    Services.scriptSecurityManager.getSystemPrincipal(),
    null,
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
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
  let uri = proxyURI.mutate().setScheme("https").finalize();

  let proxyFlags =
    Ci.nsIProtocolProxyService.RESOLVE_PREFER_SOCKS_PROXY |
    Ci.nsIProtocolProxyService.RESOLVE_PREFER_HTTPS_PROXY |
    Ci.nsIProtocolProxyService.RESOLVE_ALWAYS_TUNNEL;

  let chan = Services.io.newChannelFromURIWithProxyFlags(
    uri,
    proxyURI,
    proxyFlags,
    null,
    Services.scriptSecurityManager.getSystemPrincipal(),
    null,
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
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

  let chan = Services.io.newChannelFromURIWithProxyFlags(
    uri,
    null,
    proxyFlags,
    null,
    Services.scriptSecurityManager.getSystemPrincipal(),
    null,
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    Ci.nsIContentPolicy.TYPE_OTHER
  );

  let pi = await TestProxyType(chan, proxyFlags);
  equal(pi.host, "localhost", "Expected proxy host to be localhost");
  equal(pi.port, 8080, "Expected proxy port to be 8080");
  equal(pi.type, "https", "Expected proxy type to be https");
});

add_task(async function testProxyHttpsToHttpIsBlocked() {
  // Ensure that regressions of bug 1702417 will be detected by the next test
  const turnUri = Services.io.newURI("http://turn.example.com/");
  const proxyFlags =
    Ci.nsIProtocolProxyService.RESOLVE_PREFER_HTTPS_PROXY |
    Ci.nsIProtocolProxyService.RESOLVE_ALWAYS_TUNNEL;

  const fakeContentPrincipal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "https://example.com"
    );

  const chan = Services.io.newChannelFromURIWithProxyFlags(
    turnUri,
    null,
    proxyFlags,
    null,
    fakeContentPrincipal,
    fakeContentPrincipal,
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    Ci.nsIContentPolicy.TYPE_OTHER
  );

  const pi = await TestProxyType(chan, proxyFlags);
  equal(pi.host, "localhost", "Expected proxy host to be localhost");
  equal(pi.port, 8080, "Expected proxy port to be 8080");
  equal(pi.type, "https", "Expected proxy type to be https");

  const csm = Cc["@mozilla.org/contentsecuritymanager;1"].getService(
    Ci.nsIContentSecurityManager
  );

  try {
    csm.performSecurityCheck(chan, null);
    Assert.ok(
      false,
      "performSecurityCheck should fail (due to mixed content blocking)"
    );
  } catch (e) {
    Assert.equal(
      e.result,
      Cr.NS_ERROR_CONTENT_BLOCKED,
      "performSecurityCheck should throw NS_ERROR_CONTENT_BLOCKED"
    );
  }
});

add_task(async function testProxyHttpsToTurnTcpWorks() {
  // Test for bug 1702417
  const turnUri = Services.io.newURI("http://turn.example.com/");
  const proxyFlags =
    Ci.nsIProtocolProxyService.RESOLVE_PREFER_HTTPS_PROXY |
    Ci.nsIProtocolProxyService.RESOLVE_ALWAYS_TUNNEL;

  const fakeContentPrincipal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      "https://example.com"
    );

  const chan = Services.io.newChannelFromURIWithProxyFlags(
    turnUri,
    null,
    proxyFlags,
    null,
    fakeContentPrincipal,
    fakeContentPrincipal,
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    // This is what allows this to avoid mixed content blocking
    Ci.nsIContentPolicy.TYPE_PROXIED_WEBRTC_MEDIA
  );

  const pi = await TestProxyType(chan, proxyFlags);
  equal(pi.host, "localhost", "Expected proxy host to be localhost");
  equal(pi.port, 8080, "Expected proxy port to be 8080");
  equal(pi.type, "https", "Expected proxy type to be https");

  const csm = Cc["@mozilla.org/contentsecuritymanager;1"].getService(
    Ci.nsIContentSecurityManager
  );

  csm.performSecurityCheck(chan, null);
  Assert.ok(true, "performSecurityCheck should succeed");
});
