/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_channels.js */
/* import-globals-from head_servers.js */
/* import-globals-from head_websocket.js */

// These test should basically match the ones in test_websocket_server.js,
// but with multiple websocket clients making requests on the same server

const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);

// setup
add_setup(async function setup() {
  // turn off cert checking for these tests
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );
});

// append cleanup to cleanup queue
registerCleanupFunction(async () => {
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    false
  );
  Services.prefs.clearUserPref("network.http.http2.websockets");
});

async function spinup_and_check(proxy_kind, ws_kind) {
  let ws_h2 = true;
  if (ws_kind == NodeWebSocketServer) {
    info("not h2 ws");
    ws_h2 = false;
  }
  Services.prefs.setBoolPref("network.http.http2.websockets", ws_h2);

  let proxy;
  if (proxy_kind) {
    proxy = new proxy_kind();
    await proxy.start();
    registerCleanupFunction(async () => proxy.stop());
  }

  let wss = new ws_kind();
  await wss.start();
  registerCleanupFunction(async () => wss.stop());

  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });
  let url = `wss://localhost:${wss.port()}`;

  let conn1 = new WebSocketConnection();
  await conn1.open(url);

  let conn2 = new WebSocketConnection();
  await conn2.open(url);

  conn1.send("msg1");
  conn2.send("msg2");

  let mess2 = await conn2.receiveMessages();
  Assert.deepEqual(mess2, ["msg2"]);

  conn1.send("msg1 again");
  let mess1 = [];
  while (mess1.length < 2) {
    // receive could return only the first or both replies.
    mess1 = mess1.concat(await conn1.receiveMessages());
  }
  Assert.deepEqual(mess1, ["msg1", "msg1 again"]);

  conn1.close();
  conn2.close();
  Assert.deepEqual({ status: Cr.NS_OK }, await conn1.finished());
  Assert.deepEqual({ status: Cr.NS_OK }, await conn2.finished());
  await wss.stop();

  if (proxy_kind) {
    await proxy.stop();
  }
}

// h1.1 direct
async function test_h1_websocket_direct() {
  await spinup_and_check(null, NodeWebSocketServer);
}

// h2 direct
async function test_h2_websocket_direct() {
  await spinup_and_check(null, NodeWebSocketHttp2Server);
}

// ws h1.1 with secure h1.1 proxy
async function test_h1_ws_with_secure_h1_proxy() {
  await spinup_and_check(NodeHTTPSProxyServer, NodeWebSocketServer);
}

// ws h1.1 with insecure h1.1 proxy
async function test_h1_ws_with_insecure_h1_proxy() {
  await spinup_and_check(NodeHTTPProxyServer, NodeWebSocketServer);
}

// ws h1.1 with h2 proxy
async function test_h1_ws_with_h2_proxy() {
  await spinup_and_check(NodeHTTP2ProxyServer, NodeWebSocketServer);
}

// ws h2 with insecure h1.1 proxy
async function test_h2_ws_with_insecure_h1_proxy() {
  await spinup_and_check(NodeHTTPProxyServer, NodeWebSocketHttp2Server);
}

// ws h2 with secure h1 proxy
async function test_h2_ws_with_secure_h1_proxy() {
  await spinup_and_check(NodeHTTPSProxyServer, NodeWebSocketHttp2Server);
}

// ws h2 with secure h2 proxy
async function test_h2_ws_with_h2_proxy() {
  await spinup_and_check(NodeHTTP2ProxyServer, NodeWebSocketHttp2Server);
}

add_task(test_h1_websocket_direct);
add_task(test_h2_websocket_direct);
add_task(test_h1_ws_with_secure_h1_proxy);
add_task(test_h1_ws_with_insecure_h1_proxy);
add_task(test_h1_ws_with_h2_proxy);

// any multi-client test with h2 websocket and any kind of proxy will fail/hang
add_task(test_h2_ws_with_insecure_h1_proxy);
add_task(test_h2_ws_with_secure_h1_proxy);
add_task(test_h2_ws_with_h2_proxy);
