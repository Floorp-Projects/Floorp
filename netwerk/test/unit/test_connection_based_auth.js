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

add_task(async function test_connection_based_auth() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  let proxy = new NodeHTTPSProxyServer();
  await proxy.start();

  await proxy.registerConnectHandler((req, clientSocket, head) => {
    if (!req.headers["proxy-authorization"]) {
      clientSocket.write(
        "HTTP/1.1 407 Unauthorized\r\n" +
          "Proxy-agent: Node.js-Proxy\r\n" +
          "Connection: keep-alive\r\n" +
          "Proxy-Authenticate: mock_auth\r\n" +
          "Content-Length: 0\r\n" +
          "\r\n"
      );

      clientSocket.on("data", data => {
        let array = data.toString().split("\r\n");
        let proxyAuthorization = "";
        for (let line of array) {
          let pair = line.split(":").map(element => element.trim());
          if (pair[0] === "Proxy-Authorization") {
            proxyAuthorization = pair[1];
          }
        }

        if (proxyAuthorization === "moz_test_credentials") {
          // We don't return 200 OK here, because we don't have a server
          // to connect to.
          clientSocket.write(
            "HTTP/1.1 404 Not Found\r\nProxy-agent: Node.js-Proxy\r\n\r\n"
          );
        } else {
          clientSocket.write(
            "HTTP/1.1 502 Error\r\nProxy-agent: Node.js-Proxy\r\n\r\n"
          );
        }
        clientSocket.destroy();
      });
      return;
    }

    // We should not reach here.
    clientSocket.write(
      "HTTP/1.1 502 Error\r\nProxy-agent: Node.js-Proxy\r\n\r\n"
    );
    clientSocket.destroy();
  });

  let chan = makeChan(`https://example.ntlm.com/test`);
  let [req] = await channelOpenPromise(chan, CL_EXPECT_FAILURE);
  Assert.equal(req.status, Cr.NS_ERROR_UNKNOWN_HOST);
  req.QueryInterface(Ci.nsIProxiedChannel);
  Assert.equal(req.httpProxyConnectResponseCode, 404);

  await proxy.stop();
});
