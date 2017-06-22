// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/

"use strict";

// Tests that nsIHttpChannelInternal.beConservative correctly limits the use of
// advanced TLS features that may cause compatibility issues. Does so by
// starting a TLS server that requires the advanced features and then ensuring
// that a client that is set to be conservative will fail when connecting.

const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
const { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm", {});
const { Promise: promise } =
  Cu.import("resource://gre/modules/Promise.jsm", {});

// Get a profile directory and ensure PSM initializes NSS.
do_get_profile();
Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);

function getCert() {
  let deferred = promise.defer();
  let certService = Cc["@mozilla.org/security/local-cert-service;1"]
                      .getService(Ci.nsILocalCertService);
  certService.getOrCreateCert("beConservative-test", {
    handleCert: function(c, rv) {
      if (rv) {
        deferred.reject(rv);
        return;
      }
      deferred.resolve(c);
    }
  });
  return deferred.promise;
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

function startServer(cert, minServerVersion, maxServerVersion) {
  let tlsServer = Cc["@mozilla.org/network/tls-server-socket;1"]
                    .createInstance(Ci.nsITLSServerSocket);
  tlsServer.init(-1, true, -1);
  tlsServer.serverCert = cert;
  tlsServer.setVersionRange(minServerVersion, maxServerVersion);
  tlsServer.setSessionCache(false);
  tlsServer.setSessionTickets(false);
  tlsServer.asyncListen(new ServerSocketListener());
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

function startClient(port, beConservative, expectSuccess) {
  let req = new XMLHttpRequest();
  req.open("GET", `https://${hostname}:${port}`);
  let internalChannel = req.channel.QueryInterface(Ci.nsIHttpChannelInternal);
  internalChannel.beConservative = beConservative;
  let deferred = promise.defer();
  req.onload = () => {
    ok(expectSuccess,
       `should ${expectSuccess ? "" : "not "}have gotten load event`);
    equal(req.responseText, "OK", "response text should be 'OK'");
    deferred.resolve();
  };
  req.onerror = () => {
    ok(!expectSuccess,
       `should ${!expectSuccess ? "" : "not "}have gotten an error`);
    deferred.resolve();
  };

  req.send();
  return deferred.promise;
}

add_task(async function() {
  Services.prefs.setIntPref("security.tls.version.max", 4);
  Services.prefs.setCharPref("network.dns.localDomains", hostname);
  let cert = await getCert();

  // First run a server that accepts TLS 1.2 and 1.3. A conservative client
  // should succeed in connecting.
  let server = startServer(cert, Ci.nsITLSClientStatus.TLS_VERSION_1_2,
                           Ci.nsITLSClientStatus.TLS_VERSION_1_3);
  storeCertOverride(server.port, cert);
  await startClient(server.port, true /*be conservative*/,
                    true /*should succeed*/);
  server.close();

  // Now run a server that only accepts TLS 1.3. A conservative client will not
  // succeed in this case.
  server = startServer(cert, Ci.nsITLSClientStatus.TLS_VERSION_1_3,
                       Ci.nsITLSClientStatus.TLS_VERSION_1_3);
  storeCertOverride(server.port, cert);
  await startClient(server.port, true /*be conservative*/,
                    false /*should fail*/);

  // However, a non-conservative client should succeed.
  await startClient(server.port, false /*don't be conservative*/,
                    true /*should succeed*/);
  server.close();
});

do_register_cleanup(function() {
  Services.prefs.clearUserPref("security.tls.version.max");
  Services.prefs.clearUserPref("network.dns.localDomains");
});
