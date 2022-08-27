// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

"use strict";

// This test ensures that in-progress https connections are cancelled when the
// user logs out of a PKCS#11 token.

// Get a profile directory and ensure PSM initializes NSS.
do_get_profile();
Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);

function getTestServerCertificate() {
  const certDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  const certFile = do_get_file("test_certDB_import/encrypted_with_aes.p12");
  certDB.importPKCS12File(certFile, "password");
  for (const cert of certDB.getCerts()) {
    if (cert.commonName == "John Doe") {
      return cert;
    }
  }
  return null;
}

class InputStreamCallback {
  constructor(output) {
    this.output = output;
    this.stopped = false;
  }

  onInputStreamReady(stream) {
    info("input stream ready");
    if (this.stopped) {
      info("input stream callback stopped - bailing");
      return;
    }
    let available = 0;
    try {
      available = stream.available();
    } catch (e) {
      // onInputStreamReady may fire when the stream has been closed.
      equal(
        e.result,
        Cr.NS_BASE_STREAM_CLOSED,
        "error should be NS_BASE_STREAM_CLOSED"
      );
    }
    if (available > 0) {
      let request = NetUtil.readInputStreamToString(stream, available, {
        charset: "utf8",
      });
      ok(
        request.startsWith("GET / HTTP/1.1\r\n"),
        "Should get a simple GET / HTTP/1.1 request"
      );
      let response =
        "HTTP/1.1 200 OK\r\n" +
        "Content-Length: 2\r\n" +
        "Content-Type: text/plain\r\n" +
        "\r\nOK";
      let written = this.output.write(response, response.length);
      equal(
        written,
        response.length,
        "should have been able to write entire response"
      );
    }
    this.output.close();
    info("done with input stream ready");
  }

  stop() {
    this.stopped = true;
    this.output.close();
  }
}

class TLSServerSecurityObserver {
  constructor(input, output) {
    this.input = input;
    this.output = output;
    this.callbacks = [];
    this.stopped = false;
  }

  onHandshakeDone(socket, status) {
    info("TLS handshake done");
    info(`TLS version used: ${status.tlsVersionUsed}`);

    if (this.stopped) {
      info("handshake done callback stopped - bailing");
      return;
    }

    let callback = new InputStreamCallback(this.output);
    this.callbacks.push(callback);
    this.input.asyncWait(callback, 0, 0, Services.tm.currentThread);

    // We've set up everything needed for a successful request/response,
    // but calling logoutAndTeardown should cause the request to be cancelled.
    Cc["@mozilla.org/security/sdr;1"]
      .getService(Ci.nsISecretDecoderRing)
      .logoutAndTeardown();
  }

  stop() {
    this.stopped = true;
    this.input.close();
    this.output.close();
    this.callbacks.forEach(callback => {
      callback.stop();
    });
  }
}

class ServerSocketListener {
  constructor() {
    this.securityObservers = [];
  }

  onSocketAccepted(socket, transport) {
    info("accepted TLS client connection");
    let connectionInfo = transport.securityCallbacks.getInterface(
      Ci.nsITLSServerConnectionInfo
    );
    let input = transport.openInputStream(0, 0, 0);
    let output = transport.openOutputStream(0, 0, 0);
    let securityObserver = new TLSServerSecurityObserver(input, output);
    this.securityObservers.push(securityObserver);
    connectionInfo.setSecurityObserver(securityObserver);
  }

  // For some reason we get input stream callback events after we've stopped
  // listening, so this ensures we just drop those events.
  onStopListening() {
    info("onStopListening");
    this.securityObservers.forEach(observer => {
      observer.stop();
    });
  }
}

function getStartedServer(cert) {
  let tlsServer = Cc["@mozilla.org/network/tls-server-socket;1"].createInstance(
    Ci.nsITLSServerSocket
  );
  tlsServer.init(-1, true, -1);
  tlsServer.serverCert = cert;
  tlsServer.setSessionTickets(false);
  tlsServer.asyncListen(new ServerSocketListener());
  return tlsServer;
}

const hostname = "example.com";

function storeCertOverride(port, cert) {
  let certOverrideService = Cc[
    "@mozilla.org/security/certoverride;1"
  ].getService(Ci.nsICertOverrideService);
  certOverrideService.rememberValidityOverride(hostname, port, {}, cert, true);
}

function startClient(port) {
  let req = new XMLHttpRequest();
  req.open("GET", `https://${hostname}:${port}`);
  return new Promise((resolve, reject) => {
    req.onload = () => {
      ok(false, "should not have gotten load event");
      resolve();
    };
    req.onerror = () => {
      ok(true, "should have gotten an error");
      resolve();
    };

    req.send();
  });
}

add_task(async function() {
  Services.prefs.setCharPref("network.dns.localDomains", hostname);
  let cert = getTestServerCertificate();

  let server = getStartedServer(cert);
  storeCertOverride(server.port, cert);
  await startClient(server.port);
  server.close();
});

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("network.dns.localDomains");
});
