/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

function channelOpenPromise(chan, flags) {
  return new Promise(resolve => {
    function finish(req, buffer) {
      resolve([req, buffer]);
    }
    chan.asyncOpen(new ChannelListener(finish, null, flags));
  });
}

async function registerSimplePathHandler(server, path) {
  return server.registerPathHandler(path, (req, resp) => {
    resp.writeHead(200);
    resp.end("done");
  });
}

add_task(async function test_verify_traffic_for_http2() {
  Services.prefs.setBoolPref(
    "network.http.http2.move_to_pending_list_after_network_change",
    true
  );

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  let server = new NodeHTTP2Server();
  await server.start();
  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref(
      "network.http.http2.move_to_pending_list_after_network_change"
    );
    await server.stop();
  });

  try {
    await server.registerPathHandler("/longDelay", (req, resp) => {
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout, no-undef
      setTimeout(function () {
        resp.writeHead(200);
        resp.end("done");
      }, 8000);
    });
  } catch (e) {}

  await registerSimplePathHandler(server, "/test");

  // Send some requests and check if we have only one h2 session.
  for (let i = 0; i < 2; i++) {
    let chan = makeChan(`https://localhost:${server.port()}/test`);
    await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
  }
  let sessionCount = await server.sessionCount();
  Assert.equal(sessionCount, 1);

  let res = await new Promise(resolve => {
    // Create a request that takes 8s to finish.
    let chan = makeChan(`https://localhost:${server.port()}/longDelay`);
    chan.asyncOpen(new ChannelListener(resolve, null, CL_ALLOW_UNKNOWN_CL));

    // Send a network change event to trigger VerifyTraffic(). After this,
    // the connnection will be put in the pending list.
    // We'll crate a new connection for the new request.
    Services.obs.notifyObservers(
      null,
      "network:link-status-changed",
      "changed"
    );

    // This request will use the new connection.
    let chan1 = makeChan(`https://localhost:${server.port()}/test`);
    chan1.asyncOpen(new ChannelListener(() => {}, null, CL_ALLOW_UNKNOWN_CL));
  });

  // The connection in the pending list should be still working.
  Assert.equal(res.status, Cr.NS_OK);
  Assert.equal(res.QueryInterface(Ci.nsIHttpChannel).responseStatus, 200);

  sessionCount = await server.sessionCount();
  Assert.equal(sessionCount, 2);

  await server.stop();
});
