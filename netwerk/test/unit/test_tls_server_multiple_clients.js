/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Need profile dir to store the key / cert
do_get_profile();
// Ensure PSM is initialized
Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);

const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);
const certService = Cc["@mozilla.org/security/local-cert-service;1"].getService(
  Ci.nsILocalCertService
);
const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);
const socketTransportService = Cc[
  "@mozilla.org/network/socket-transport-service;1"
].getService(Ci.nsISocketTransportService);

function getCert() {
  return new Promise((resolve, reject) => {
    certService.getOrCreateCert("tls-test", {
      handleCert(c, rv) {
        if (rv) {
          reject(rv);
          return;
        }
        resolve(c);
      },
    });
  });
}

function startServer(cert) {
  let tlsServer = Cc["@mozilla.org/network/tls-server-socket;1"].createInstance(
    Ci.nsITLSServerSocket
  );
  tlsServer.init(-1, true, -1);
  tlsServer.serverCert = cert;

  let input, output;

  let listener = {
    onSocketAccepted(socket, transport) {
      info("Accept TLS client connection");
      let connectionInfo = transport.securityInfo.QueryInterface(
        Ci.nsITLSServerConnectionInfo
      );
      connectionInfo.setSecurityObserver(listener);
      input = transport.openInputStream(0, 0, 0);
      output = transport.openOutputStream(0, 0, 0);
    },
    onHandshakeDone(socket, status) {
      info("TLS handshake done");

      input.asyncWait(
        {
          onInputStreamReady(input) {
            NetUtil.asyncCopy(input, output);
          },
        },
        0,
        0,
        Services.tm.currentThread
      );
    },
    onStopListening() {},
  };

  tlsServer.setSessionTickets(false);

  tlsServer.asyncListen(listener);

  return tlsServer.port;
}

function storeCertOverride(port, cert) {
  let overrideBits =
    Ci.nsICertOverrideService.ERROR_UNTRUSTED |
    Ci.nsICertOverrideService.ERROR_MISMATCH;
  certOverrideService.rememberValidityOverride(
    "127.0.0.1",
    port,
    {},
    cert,
    overrideBits,
    true
  );
}

function startClient(port) {
  let transport = socketTransportService.createTransport(
    ["ssl"],
    "127.0.0.1",
    port,
    null,
    null
  );
  let input;
  let output;

  let inputDeferred = PromiseUtils.defer();
  let outputDeferred = PromiseUtils.defer();

  let handler = {
    onTransportStatus(transport, status) {
      if (status === Ci.nsISocketTransport.STATUS_CONNECTED_TO) {
        output.asyncWait(handler, 0, 0, Services.tm.currentThread);
      }
    },

    onInputStreamReady(input) {
      try {
        let data = NetUtil.readInputStreamToString(input, input.available());
        equal(data, "HELLO", "Echoed data received");
        input.close();
        output.close();
        inputDeferred.resolve();
      } catch (e) {
        inputDeferred.reject(e);
      }
    },

    onOutputStreamReady(output) {
      try {
        output.write("HELLO", 5);
        info("Output to server written");
        outputDeferred.resolve();
        input = transport.openInputStream(0, 0, 0);
        input.asyncWait(handler, 0, 0, Services.tm.currentThread);
      } catch (e) {
        outputDeferred.reject(e);
      }
    },
  };

  transport.setEventSink(handler, Services.tm.currentThread);
  output = transport.openOutputStream(0, 0, 0);

  return Promise.all([inputDeferred.promise, outputDeferred.promise]);
}

add_task(async function() {
  let cert = await getCert();
  ok(!!cert, "Got self-signed cert");
  let port = startServer(cert);
  storeCertOverride(port, cert);
  await startClient(port);
  await startClient(port);
});
