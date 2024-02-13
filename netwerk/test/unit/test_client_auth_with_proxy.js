/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_channels.js */
/* import-globals-from head_servers.js */

const { MockRegistrar } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistrar.sys.mjs"
);

const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);

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

class SecurityObserver {
  constructor(input, output) {
    this.input = input;
    this.output = output;
  }

  onHandshakeDone(socket, status) {
    info("TLS handshake done");

    let output = this.output;
    this.input.asyncWait(
      {
        onInputStreamReady(readyInput) {
          let request = NetUtil.readInputStreamToString(
            readyInput,
            readyInput.available()
          );
          ok(
            request.startsWith("GET /") && request.includes("HTTP/1.1"),
            "expecting an HTTP/1.1 GET request"
          );
          let response =
            "HTTP/1.1 200 OK\r\nContent-Type:text/plain\r\n" +
            "Connection:Close\r\nContent-Length:2\r\n\r\nOK";
          output.write(response, response.length);
        },
      },
      0,
      0,
      Services.tm.currentThread
    );
  }
}

function startServer(cert) {
  let tlsServer = Cc["@mozilla.org/network/tls-server-socket;1"].createInstance(
    Ci.nsITLSServerSocket
  );
  tlsServer.init(-1, true, -1);
  tlsServer.serverCert = cert;

  let securityObservers = [];

  let listener = {
    onSocketAccepted(socket, transport) {
      info("Accepted TLS client connection");
      let connectionInfo = transport.securityCallbacks.getInterface(
        Ci.nsITLSServerConnectionInfo
      );
      let input = transport.openInputStream(0, 0, 0);
      let output = transport.openOutputStream(0, 0, 0);
      connectionInfo.setSecurityObserver(new SecurityObserver(input, output));
    },

    onStopListening() {
      info("onStopListening");
      for (let securityObserver of securityObservers) {
        securityObserver.input.close();
        securityObserver.output.close();
      }
    },
  };

  tlsServer.setSessionTickets(false);
  tlsServer.setRequestClientCertificate(Ci.nsITLSServerSocket.REQUEST_ALWAYS);

  tlsServer.asyncListen(listener);

  return tlsServer;
}

// Replace the UI dialog that prompts the user to pick a client certificate.
const clientAuthDialogService = {
  chooseCertificate(hostname, certArray, loadContext, callback) {
    callback.certificateChosen(certArray[0], false);
  },
  QueryInterface: ChromeUtils.generateQI(["nsIClientAuthDialogService"]),
};

let server;
add_setup(async function setup() {
  do_get_profile();

  let clientAuthDialogServiceCID = MockRegistrar.register(
    "@mozilla.org/security/ClientAuthDialogService;1",
    clientAuthDialogService
  );

  let cert = getTestServerCertificate();
  ok(!!cert, "Got self-signed cert");
  server = startServer(cert);

  certOverrideService.rememberValidityOverride(
    "localhost",
    server.port,
    {},
    cert,
    true
  );

  registerCleanupFunction(async function () {
    MockRegistrar.unregister(clientAuthDialogServiceCID);
    certOverrideService.clearValidityOverride("localhost", server.port, {});
    server.close();
  });
});

add_task(async function test_client_auth_with_proxy() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");
  addCertFromFile(certdb, "proxy-ca.pem", "CTu,u,u");

  let proxies = [
    NodeHTTPProxyServer,
    NodeHTTPSProxyServer,
    NodeHTTP2ProxyServer,
  ];

  for (let p of proxies) {
    info(`Test with proxy:${p.name}`);
    let proxy = new p();
    await proxy.start();
    registerCleanupFunction(async () => {
      await proxy.stop();
    });

    let chan = makeChan(`https://localhost:${server.port}`);
    let [req, buff] = await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
    equal(req.status, Cr.NS_OK);
    equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 200);
    equal(buff, "OK");
    req.QueryInterface(Ci.nsIProxiedChannel);
    ok(!!req.proxyInfo);
    notEqual(req.proxyInfo.type, "direct");
    await proxy.stop();
  }
});
