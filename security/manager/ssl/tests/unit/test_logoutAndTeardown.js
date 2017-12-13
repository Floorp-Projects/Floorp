// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

"use strict";

// This test ensures that in-progress https connections are cancelled when the
// user logs out of a PKCS#11 token.

// Get a profile directory and ensure PSM initializes NSS.
do_get_profile();
Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);

function getCert() {
  return new Promise((resolve, reject) => {
    let certService = Cc["@mozilla.org/security/local-cert-service;1"]
                        .getService(Ci.nsILocalCertService);
    certService.getOrCreateCert("beConservative-test", {
      handleCert: (c, rv) => {
        if (rv) {
          reject(rv);
          return;
        }
        resolve(c);
      }
    });
  });
}

class InputStreamCallback {
  constructor(output) {
    this.output = output;
    this.stopped = false;
  }

  onInputStreamReady(stream) {
    do_print("input stream ready");
    if (this.stopped) {
      do_print("input stream callback stopped - bailing");
      return;
    }
    let available = 0;
    try {
      available = stream.available();
    } catch (e) {
      // onInputStreamReady may fire when the stream has been closed.
      equal(e.result, Cr.NS_BASE_STREAM_CLOSED,
            "error should be NS_BASE_STREAM_CLOSED");
    }
    if (available > 0) {
      let request = NetUtil.readInputStreamToString(stream, available,
                                                    { charset: "utf8" });
      ok(request.startsWith("GET / HTTP/1.1\r\n"),
         "Should get a simple GET / HTTP/1.1 request");
      let response = "HTTP/1.1 200 OK\r\n" +
                     "Content-Length: 2\r\n" +
                     "Content-Type: text/plain\r\n" +
                     "\r\nOK";
      let written = this.output.write(response, response.length);
      equal(written, response.length,
            "should have been able to write entire response");
    }
    this.output.close();
    do_print("done with input stream ready");
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
    do_print("TLS handshake done");
    do_print(`TLS version used: ${status.tlsVersionUsed}`);

    if (this.stopped) {
      do_print("handshake done callback stopped - bailing");
      return;
    }

    let callback = new InputStreamCallback(this.output);
    this.callbacks.push(callback);
    this.input.asyncWait(callback, 0, 0, Services.tm.currentThread);

    // We've set up everything needed for a successful request/response,
    // but calling logoutAndTeardown should cause the request to be cancelled.
    Cc["@mozilla.org/security/sdr;1"].getService(Ci.nsISecretDecoderRing)
      .logoutAndTeardown();
  }

  stop() {
    this.stopped = true;
    this.input.close();
    this.output.close();
    this.callbacks.forEach((callback) => {
      callback.stop();
    });
  }
}

class ServerSocketListener {
  constructor() {
    this.securityObservers = [];
  }

  onSocketAccepted(socket, transport) {
    do_print("accepted TLS client connection");
    let connectionInfo = transport.securityInfo
                         .QueryInterface(Ci.nsITLSServerConnectionInfo);
    let input = transport.openInputStream(0, 0, 0);
    let output = transport.openOutputStream(0, 0, 0);
    let securityObserver = new TLSServerSecurityObserver(input, output);
    this.securityObservers.push(securityObserver);
    connectionInfo.setSecurityObserver(securityObserver);
  }

  // For some reason we get input stream callback events after we've stopped
  // listening, so this ensures we just drop those events.
  onStopListening() {
    do_print("onStopListening");
    this.securityObservers.forEach((observer) => {
      observer.stop();
    });
  }
}

function getStartedServer(cert) {
  let tlsServer = Cc["@mozilla.org/network/tls-server-socket;1"]
                    .createInstance(Ci.nsITLSServerSocket);
  tlsServer.init(-1, true, -1);
  tlsServer.serverCert = cert;
  tlsServer.setSessionCache(false);
  tlsServer.setSessionTickets(false);
  tlsServer.asyncListen(new ServerSocketListener());
  return tlsServer;
}

const hostname = "example.com";

function storeCertOverride(port, cert) {
  let certOverrideService = Cc["@mozilla.org/security/certoverride;1"]
                              .getService(Ci.nsICertOverrideService);
  let overrideBits = Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                     Ci.nsICertOverrideService.ERROR_MISMATCH;
  certOverrideService.rememberValidityOverride(hostname, port, cert,
                                               overrideBits, true);
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
  let cert = await getCert();

  let server = getStartedServer(cert);
  storeCertOverride(server.port, cert);
  await startClient(server.port);
  server.close();
});

do_register_cleanup(function() {
  Services.prefs.clearUserPref("network.dns.localDomains");
});
