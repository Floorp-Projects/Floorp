/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Summary:
// Test whether the connection limit is honored when http2 proxy is used.
//
// Test step:
// 1. Create 30 http requests.
// 2. Check if the count of all sockets created by proxy is less than 6.

"use strict";

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_channels.js */
/* import-globals-from head_servers.js */

function makeChan(uri) {
  let chan = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return chan;
}

add_task(async function test_connection_limit() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  let proxy = new NodeHTTP2ProxyServer();
  await proxy.start();
  registerCleanupFunction(async () => {
    await proxy.stop();
  });

  const maxConnections = 6;
  Services.prefs.setIntPref(
    "network.http.max-persistent-connections-per-server",
    maxConnections
  );
  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref(
      "network.http.max-persistent-connections-per-server"
    );
  });

  await with_node_servers([NodeHTTP2Server], async server => {
    await server.registerPathHandler("/test", (req, resp) => {
      resp.writeHead(200);
      resp.end("All good");
    });

    let promises = [];
    for (let i = 0; i < 30; ++i) {
      let chan = makeChan(`${server.origin()}/test`);
      promises.push(
        new Promise(resolve => {
          chan.asyncOpen(
            new ChannelListener(resolve, null, CL_ALLOW_UNKNOWN_CL)
          );
        })
      );
    }
    await Promise.all(promises);
    let count = await proxy.socketCount(server.port());
    Assert.lessOrEqual(
      count,
      maxConnections,
      "socket count should be less than maxConnections"
    );
  });
});
