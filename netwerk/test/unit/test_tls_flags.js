// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

"use strict";

// a fork of test_be_conservative

// Tests that nsIHttpChannelInternal.tlsFlags can be used to set the
// client max version level. Flags can also be used to set the
// level of intolerance rollback and to test out an experimental 1.3
// hello, though they are not tested here.

const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
const { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm", {});

// Get a profile directory and ensure PSM initializes NSS.
do_get_profile();
Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);

function getCert() {
  return new Promise((resolve, reject) => {
    let certService = Cc["@mozilla.org/security/local-cert-service;1"]
                        .getService(Ci.nsILocalCertService);
    certService.getOrCreateCert("tlsflags-test", {
      handleCert: function(c, rv) {
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
      equal(e.result, Cr.NS_BASE_STREAM_CLOSED,
            "error should be NS_BASE_STREAM_CLOSED");
    }
    if (available > 0) {
      let request = NetUtil.readInputStreamToString(stream, available,
                                                    { charset: "utf8"});
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
    info("done with input stream ready");
  }

  stop() {
    this.stopped = true;
    this.output.close();
  }
}

class TLSServerSecurityObserver {
  constructor(input, output, expectedVersion) {
    this.input = input;
    this.output = output;
    this.expectedVersion = expectedVersion;
    this.callbacks = [];
    this.stopped = false;
  }

  onHandshakeDone(socket, status) {
    info("TLS handshake done");
    info(`TLS version used: ${status.tlsVersionUsed}`);
    info(this.expectedVersion);
    equal(status.tlsVersionUsed, this.expectedVersion, "expected version check");
    if (this.stopped) {
      info("handshake done callback stopped - bailing");
      return;
    }

    let callback = new InputStreamCallback(this.output);
    this.callbacks.push(callback);
    this.input.asyncWait(callback, 0, 0, Services.tm.currentThread);
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


function startServer(cert, minServerVersion, maxServerVersion, expectedVersion) {
  let tlsServer = Cc["@mozilla.org/network/tls-server-socket;1"]
                    .createInstance(Ci.nsITLSServerSocket);
  tlsServer.init(-1, true, -1);
  tlsServer.serverCert = cert;
  tlsServer.setVersionRange(minServerVersion, maxServerVersion);
  tlsServer.setSessionCache(false);
  tlsServer.setSessionTickets(false);

  let listener = {
    securityObservers : [],

    onSocketAccepted: function(socket, transport) {
      info("accepted TLS client connection");
      let connectionInfo = transport.securityInfo
          .QueryInterface(Ci.nsITLSServerConnectionInfo);
      let input = transport.openInputStream(0, 0, 0);
      let output = transport.openOutputStream(0, 0, 0);
      let securityObserver = new TLSServerSecurityObserver(input, output, expectedVersion);
      this.securityObservers.push(securityObserver);
      connectionInfo.setSecurityObserver(securityObserver);
    },

    // For some reason we get input stream callback events after we've stopped
    // listening, so this ensures we just drop those events.
    onStopListening: function() {
      info("onStopListening");
      this.securityObservers.forEach((observer) => {
        observer.stop();
      });
    }
  }
  tlsServer.asyncListen(listener);
  return tlsServer;
}

const hostname = "example.com"

function storeCertOverride(port, cert) {
  let certOverrideService = Cc["@mozilla.org/security/certoverride;1"]
                              .getService(Ci.nsICertOverrideService);
  let overrideBits = Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                     Ci.nsICertOverrideService.ERROR_MISMATCH;
  certOverrideService.rememberValidityOverride(hostname, port, cert,
                                               overrideBits, true);
}

function startClient(port, tlsFlags, expectSuccess) {
  let req = new XMLHttpRequest();
  req.open("GET", `https://${hostname}:${port}`);
  let internalChannel = req.channel.QueryInterface(Ci.nsIHttpChannelInternal);
  internalChannel.tlsFlags = tlsFlags;
  return new Promise((resolve, reject) => {
    req.onload = () => {
      ok(expectSuccess,
         `should ${expectSuccess ? "" : "not "}have gotten load event`);
      equal(req.responseText, "OK", "response text should be 'OK'");
      resolve();
    };
    req.onerror = () => {
      ok(!expectSuccess,
         `should ${!expectSuccess ? "" : "not "}have gotten an error`);
      resolve();
    };

    req.send();
  });
}

add_task(async function() {
  Services.prefs.setIntPref("security.tls.version.max", 4);
  Services.prefs.setCharPref("network.dns.localDomains", hostname);
  let cert = await getCert();

  // server that accepts 1.1->1.3 and a client max 1.3. expect 1.3
  info("TEST 1");
  let server = startServer(cert,
                           Ci.nsITLSClientStatus.TLS_VERSION_1_1,
                           Ci.nsITLSClientStatus.TLS_VERSION_1_3,
                           Ci.nsITLSClientStatus.TLS_VERSION_1_3);
  storeCertOverride(server.port, cert);
  await startClient(server.port, 4, true /*should succeed*/);
  server.close();

  // server that accepts 1.1->1.3 and a client max 1.1. expect 1.1
  info("TEST 2");
  server = startServer(cert,
                       Ci.nsITLSClientStatus.TLS_VERSION_1_1,
                       Ci.nsITLSClientStatus.TLS_VERSION_1_3,
                       Ci.nsITLSClientStatus.TLS_VERSION_1_1);
  storeCertOverride(server.port, cert);
  await startClient(server.port, 2, true);
  server.close();

  // server that accepts 1.2->1.2 and a client max 1.3. expect 1.2
  info("TEST 3");
  server = startServer(cert,
                       Ci.nsITLSClientStatus.TLS_VERSION_1_2,
                       Ci.nsITLSClientStatus.TLS_VERSION_1_2,
                       Ci.nsITLSClientStatus.TLS_VERSION_1_2);
  storeCertOverride(server.port, cert);
  await startClient(server.port, 4, true);
  server.close();

  // server that accepts 1.2->1.2 and a client max 1.1. expect fail
  info("TEST 4");
  server = startServer(cert,
                       Ci.nsITLSClientStatus.TLS_VERSION_1_2,
                       Ci.nsITLSClientStatus.TLS_VERSION_1_2, 0);
  storeCertOverride(server.port, cert);
  await startClient(server.port, 2, false);

  server.close();
});

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("security.tls.version.max");
  Services.prefs.clearUserPref("network.dns.localDomains");
});
