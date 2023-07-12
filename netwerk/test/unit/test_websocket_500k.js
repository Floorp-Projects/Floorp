/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_channels.js */
/* import-globals-from head_servers.js */

ChromeUtils.defineESModuleGetters(this, {
  ObjectUtils: "resource://gre/modules/ObjectUtils.sys.mjs",
});

add_setup(async function () {
  Services.prefs.setCharPref("network.dns.localDomains", "foo.example.com");

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");
  addCertFromFile(certdb, "proxy-ca.pem", "CTu,u,u");
});

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("network.dns.localDomains");
});

async function channelOpenPromise(url, msg) {
  let conn = new WebSocketConnection();
  await conn.open(url);
  conn.send(msg);
  let res = await conn.receiveMessages();
  conn.close();
  let { status } = await conn.finished();
  return [status, res];
}

async function sendDataAndCheck(url) {
  let data = "a".repeat(500000);
  let [status, res] = await channelOpenPromise(url, data);
  Assert.equal(status, Cr.NS_OK);
  // Use "ObjectUtils.deepEqual" directly to avoid printing data.
  Assert.ok(ObjectUtils.deepEqual(res, [data]));
}

add_task(async function test_h2_websocket_500k() {
  Services.prefs.setBoolPref("network.http.http2.websockets", true);
  let wss = new NodeWebSocketHttp2Server();
  await wss.start();
  registerCleanupFunction(async () => wss.stop());

  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });
  let url = `wss://foo.example.com:${wss.port()}`;
  await sendDataAndCheck(url);
});

// h1.1 direct
add_task(async function test_h1_websocket_direct() {
  let wss = new NodeWebSocketServer();
  await wss.start();
  registerCleanupFunction(async () => wss.stop());
  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });
  let url = `wss://localhost:${wss.port()}`;
  await sendDataAndCheck(url);
});

// ws h1.1 with insecure h1.1 proxy
add_task(async function test_h1_ws_with_h1_insecure_proxy() {
  Services.prefs.setBoolPref("network.http.http2.websockets", false);
  let proxy = new NodeHTTPProxyServer();
  await proxy.start();

  let wss = new NodeWebSocketServer();
  await wss.start();

  registerCleanupFunction(async () => {
    await wss.stop();
    await proxy.stop();
  });

  Assert.notEqual(wss.port(), null);

  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });
  let url = `wss://localhost:${wss.port()}`;
  await sendDataAndCheck(url);
});

// h1 server with secure h1.1 proxy
add_task(async function test_h1_ws_with_secure_h1_proxy() {
  let proxy = new NodeHTTPSProxyServer();
  await proxy.start();

  let wss = new NodeWebSocketServer();
  await wss.start();
  registerCleanupFunction(async () => {
    await wss.stop();
    await proxy.stop();
  });

  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });

  let url = `wss://localhost:${wss.port()}`;
  await sendDataAndCheck(url);

  await proxy.stop();
});

// ws h1.1 with h2 proxy
add_task(async function test_h1_ws_with_h2_proxy() {
  Services.prefs.setBoolPref("network.http.http2.websockets", false);

  let proxy = new NodeHTTP2ProxyServer();
  await proxy.start();

  let wss = new NodeWebSocketServer();
  await wss.start();

  registerCleanupFunction(async () => {
    await wss.stop();
    await proxy.stop();
  });

  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });

  let url = `wss://localhost:${wss.port()}`;
  await sendDataAndCheck(url);

  await proxy.stop();
});

// ws h2 with insecure h1.1 proxy
add_task(async function test_h2_ws_with_h1_insecure_proxy() {
  Services.prefs.setBoolPref("network.http.http2.websockets", true);

  let proxy = new NodeHTTPProxyServer();
  await proxy.start();

  let wss = new NodeWebSocketHttp2Server();
  await wss.start();

  registerCleanupFunction(async () => {
    await wss.stop();
    await proxy.stop();
  });

  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });

  let url = `wss://localhost:${wss.port()}`;
  await sendDataAndCheck(url);

  await proxy.stop();
});

add_task(async function test_h2_ws_with_h1_secure_proxy() {
  Services.prefs.setBoolPref("network.http.http2.websockets", true);

  let proxy = new NodeHTTPSProxyServer();
  await proxy.start();

  let wss = new NodeWebSocketHttp2Server();
  await wss.start();

  registerCleanupFunction(async () => {
    await wss.stop();
    await proxy.stop();
  });

  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });

  let url = `wss://localhost:${wss.port()}`;
  await sendDataAndCheck(url);

  await proxy.stop();
});

// ws h2 with secure h2 proxy
add_task(async function test_h2_ws_with_h2_proxy() {
  Services.prefs.setBoolPref("network.http.http2.websockets", true);

  let proxy = new NodeHTTP2ProxyServer();
  await proxy.start(); // start and register proxy "filter"

  let wss = new NodeWebSocketHttp2Server();
  await wss.start(); // init port

  registerCleanupFunction(async () => {
    await wss.stop();
    await proxy.stop();
  });

  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });

  let url = `wss://localhost:${wss.port()}`;
  await sendDataAndCheck(url);

  await proxy.stop();
});
