/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_channels.js */
/* import-globals-from head_servers.js */

const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);

add_setup(async function setup() {
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );

  registerCleanupFunction(async () => {
    certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
      false
    );
  });
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

add_task(async function test_websocket() {
  let wss = new NodeWebSocketServer();
  await wss.start();
  registerCleanupFunction(async () => wss.stop());
  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });
  let url = `wss://localhost:${wss.port()}`;
  const msg = "test websocket";

  let conn = new WebSocketConnection();
  await conn.open(url);
  conn.send(msg);
  let mess1 = await conn.receiveMessages();
  Assert.deepEqual(mess1, [msg]);

  // Now send 3 more, and check that we received all of them
  conn.send(msg);
  conn.send(msg);
  conn.send(msg);
  let mess2 = [];
  while (mess2.length < 3) {
    // receive could return 1, 2 or all 3 replies.
    mess2 = mess2.concat(await conn.receiveMessages());
  }
  Assert.deepEqual(mess2, [msg, msg, msg]);

  conn.close();
  let { status } = await conn.finished();

  Assert.equal(status, Cr.NS_OK);
  await wss.stop();
});

add_task(async function test_two_clients() {
  let wss = new NodeWebSocketServer();
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
    // receive could return only the fist or both replies.
    mess1 = mess1.concat(await conn1.receiveMessages());
  }
  Assert.deepEqual(mess1, ["msg1", "msg1 again"]);

  conn1.close();
  conn2.close();
  Assert.deepEqual({ status: Cr.NS_OK }, await conn1.finished());
  Assert.deepEqual({ status: Cr.NS_OK }, await conn2.finished());
  await wss.stop();
});

add_task(async function test_ws_through_https_proxy() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");
  addCertFromFile(certdb, "proxy-ca.pem", "CTu,u,u");

  let proxy = new NodeHTTPSProxyServer();
  await proxy.start();

  let wss = new NodeWebSocketServer();
  await wss.start();
  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });

  let url = `wss://localhost:${wss.port()}`;
  const msg = "test websocket through proxy";
  let [status, res] = await channelOpenPromise(url, msg);
  Assert.equal(status, Cr.NS_OK);
  Assert.deepEqual(res, [msg]);

  await proxy.stop();
  await wss.stop();
});

add_task(async function test_websocket_over_h2() {
  Services.prefs.setBoolPref("network.http.http2.websockets", true);
  let wss = new NodeWebSocketHttp2Server();
  await wss.start();
  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });
  let url = `wss://localhost:${wss.port()}`;
  const msg = "test websocket";
  let [status, res] = await channelOpenPromise(url, msg);
  Assert.equal(status, Cr.NS_OK);
  Assert.deepEqual(res, [msg]);
  await wss.stop();
});
