/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_channels.js */
/* import-globals-from head_servers.js */
/* import-globals-from head_websocket.js */

let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

add_setup(() => {
  Services.prefs.setBoolPref("network.http.http2.websockets", true);
});

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("network.http.http2.websockets");
});

// TLS handshake to the end server fails with proxy
async function test_tls_fail_on_ws_server_over_proxy() {
  // we are expecting a timeout, so lets shorten how long we must wait
  Services.prefs.setIntPref("network.websocket.timeout.open", 1);

  // no cert to ws server
  addCertFromFile(certdb, "proxy-ca.pem", "CTu,u,u");

  let proxy = new NodeHTTPSProxyServer();
  await proxy.start();

  let wss = new NodeWebSocketServer();
  await wss.start();

  registerCleanupFunction(async () => {
    await wss.stop();
    await proxy.stop();
    Services.prefs.clearUserPref("network.websocket.timeout.open");
  });

  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler((data, ws) => {
    ws.send(data);
  });

  let chan = makeWebSocketChan();
  let url = `wss://localhost:${wss.port()}`;
  const msg = "test tls fail on ws server over proxy";
  let [status] = await openWebSocketChannelPromise(chan, url, msg);

  Assert.equal(status, Cr.NS_ERROR_NET_TIMEOUT_EXTERNAL);
}
add_task(test_tls_fail_on_ws_server_over_proxy);
