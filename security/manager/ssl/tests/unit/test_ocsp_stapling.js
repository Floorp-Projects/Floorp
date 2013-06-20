/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// In which we connect to a number of domains (as faked by a server running
// locally) with and without OCSP stapling enabled to determine that good
// things happen and bad things don't.

let { Promise } = Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js", {});
let { HttpServer } = Cu.import("resource://testing-common/httpd.js", {});

let gOCSPServerProcess = null;
let gHttpServer = null;

const REMOTE_PORT = 8443;
const CALLBACK_PORT = 8444;

function Connection(aHost) {
  this.host = aHost;
  let threadManager = Cc["@mozilla.org/thread-manager;1"]
                        .getService(Ci.nsIThreadManager);
  this.thread = threadManager.currentThread;
  this.defer = Promise.defer();
  let sts = Cc["@mozilla.org/network/socket-transport-service;1"]
              .getService(Ci.nsISocketTransportService);
  this.transport = sts.createTransport(["ssl"], 1, aHost, REMOTE_PORT, null);
  this.transport.setEventSink(this, this.thread);
  this.inputStream = null;
  this.outputStream = null;
  this.connected = false;
}

Connection.prototype = {
  // nsITransportEventSink
  onTransportStatus: function(aTransport, aStatus, aProgress, aProgressMax) {
    if (!this.connected && aStatus == Ci.nsISocketTransport.STATUS_CONNECTED_TO) {
      this.connected = true;
      this.outputStream.asyncWait(this, 0, 0, this.thread);
    }
  },

  // nsIInputStreamCallback
  onInputStreamReady: function(aStream) {
    try {
      // this will throw if the stream has been closed by an error
      let str = NetUtil.readInputStreamToString(aStream, aStream.available());
      do_check_eq(str, "0");
      this.inputStream.close();
      this.inputStream = null;
      this.outputStream.close();
      this.outputStream = null;
      this.transport = null;
      this.defer.resolve(Cr.NS_OK);
    } catch (e) {
      this.defer.resolve(e.result);
    }
  },

  // nsIOutputStreamCallback
  onOutputStreamReady: function(aStream) {
    let sslSocketControl = this.transport.securityInfo
                             .QueryInterface(Ci.nsISSLSocketControl);
    sslSocketControl.proxyStartSSL();
    this.outputStream.write("0", 1);
    let inStream = this.transport.openInputStream(0, 0, 0)
                     .QueryInterface(Ci.nsIAsyncInputStream);
    this.inputStream = inStream;
    this.inputStream.asyncWait(this, 0, 0, this.thread);
  },

  go: function() {
    this.outputStream = this.transport.openOutputStream(0, 0, 0)
                          .QueryInterface(Ci.nsIAsyncOutputStream);
    return this.defer.promise;
  }
};

/* Returns a promise to connect to aHost that resolves to the result of that
 * connection */
function connectTo(aHost) {
  Services.prefs.setCharPref("network.dns.localDomains", aHost);
  let connection = new Connection(aHost);
  return connection.go();
}

function add_connection_test(aHost, aExpectedResult, aStaplingEnabled) {
  add_test(function() {
    Services.prefs.setBoolPref("security.ssl.enable_ocsp_stapling",
                               aStaplingEnabled);
    do_test_pending();
    connectTo(aHost).then(function(aResult) {
      do_check_eq(aResult, aExpectedResult);
      do_test_finished();
      run_next_test();
    });
  });
}

function cleanup() {
  gOCSPServerProcess.kill();
}

function run_test() {
  do_get_profile();
  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);
  addCertFromFile(certdb, "test_ocsp_stapling/ocsp-ca.der", "CTu,u,u");

  let directoryService = Cc["@mozilla.org/file/directory_service;1"]
                           .getService(Ci.nsIProperties);
  let envSvc = Cc["@mozilla.org/process/environment;1"]
                 .getService(Ci.nsIEnvironment);
  let greDir = directoryService.get("GreD", Ci.nsIFile);
  envSvc.set("DYLD_LIBRARY_PATH", greDir.path);
  envSvc.set("LD_LIBRARY_PATH", greDir.path);
  envSvc.set("OCSP_SERVER_DEBUG_LEVEL", "3");
  envSvc.set("OCSP_SERVER_CALLBACK_PORT", CALLBACK_PORT);

  gHttpServer = new HttpServer();
  gHttpServer.registerPathHandler("/", handleServerCallback);
  gHttpServer.start(CALLBACK_PORT);

  let serverBin = directoryService.get("CurProcD", Ci.nsILocalFile);
  serverBin.append("OCSPStaplingServer" + (gIsWindows ? ".exe" : ""));
  // If we're testing locally, the above works. If not, the server executable
  // is in another location.
  if (!serverBin.exists()) {
    serverBin = directoryService.get("CurWorkD", Ci.nsILocalFile);
    while (serverBin.path.indexOf("xpcshell") != -1) {
      serverBin = serverBin.parent;
    }
    serverBin.append("bin");
    serverBin.append("OCSPStaplingServer" + (gIsWindows ? ".exe" : ""));
  }
  do_check_true(serverBin.exists());
  gOCSPServerProcess = Cc["@mozilla.org/process/util;1"]
                     .createInstance(Ci.nsIProcess);
  gOCSPServerProcess.init(serverBin);
  let ocspCertDir = directoryService.get("CurWorkD", Ci.nsILocalFile);
  ocspCertDir.append("test_ocsp_stapling");
  do_check_true(ocspCertDir.exists());
  gOCSPServerProcess.run(false, [ocspCertDir.path], 1);

  do_test_pending();
}

function handleServerCallback(aRequest, aResponse) {
  aResponse.write("OK!");
  aResponse.seizePower();
  aResponse.finish();
  run_test_body();
}

function run_test_body() {
  // In the absence of OCSP stapling, these should actually all work.
  add_connection_test("ocsp-stapling-good.example.com", Cr.NS_OK, false);
  add_connection_test("ocsp-stapling-revoked.example.com", Cr.NS_OK, false);
  add_connection_test("ocsp-stapling-good-other-ca.example.com", Cr.NS_OK, false);
  add_connection_test("ocsp-stapling-malformed.example.com", Cr.NS_OK, false);
  add_connection_test("ocsp-stapling-srverr.example.com", Cr.NS_OK, false);
  add_connection_test("ocsp-stapling-trylater.example.com", Cr.NS_OK, false);
  add_connection_test("ocsp-stapling-needssig.example.com", Cr.NS_OK, false);
  add_connection_test("ocsp-stapling-unauthorized.example.com", Cr.NS_OK, false);
  add_connection_test("ocsp-stapling-unknown.example.com", Cr.NS_OK, false);
  add_connection_test("ocsp-stapling-good-other.example.com", Cr.NS_OK, false);
  add_connection_test("ocsp-stapling-none.example.com", Cr.NS_OK, false);
  add_connection_test("ocsp-stapling-expired.example.com", Cr.NS_OK, false);
  add_connection_test("ocsp-stapling-expired-fresh-ca.example.com", Cr.NS_OK, false);
  // Now test OCSP stapling
  // The following error codes are defined in security/nss/lib/util/SECerrs.h
  add_connection_test("ocsp-stapling-good.example.com", Cr.NS_OK, true);
  // SEC_ERROR_REVOKED_CERTIFICATE = SEC_ERROR_BASE + 12
  add_connection_test("ocsp-stapling-revoked.example.com", getXPCOMStatusFromNSS(12), true);
  // This stapled response is from a CA that is untrusted and did not issue
  // the server's certificate.
  // SEC_ERROR_BAD_DATABASE = SEC_ERROR_BASE + 18
  add_connection_test("ocsp-stapling-good-other-ca.example.com", getXPCOMStatusFromNSS(18), true);
  // Now add that CA to the trusted database. It still should not be able
  // to sign for the ocsp response.
  add_test(function() {
    let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                   .getService(Ci.nsIX509CertDB);
    addCertFromFile(certdb, "test_ocsp_stapling/ocsp-other-ca.der", "CTu,u,u");
    run_next_test();
  });
  // SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE = (SEC_ERROR_BASE + 130)
  add_connection_test("ocsp-stapling-good-other-ca.example.com", getXPCOMStatusFromNSS(130), true);
  // SEC_ERROR_OCSP_MALFORMED_REQUEST = (SEC_ERROR_BASE + 120)
  add_connection_test("ocsp-stapling-malformed.example.com", getXPCOMStatusFromNSS(120), true);
  // SEC_ERROR_OCSP_SERVER_ERROR = (SEC_ERROR_BASE + 121)
  add_connection_test("ocsp-stapling-srverr.example.com", getXPCOMStatusFromNSS(121), true);
  // SEC_ERROR_OCSP_TRY_SERVER_LATER = (SEC_ERROR_BASE + 122)
  add_connection_test("ocsp-stapling-trylater.example.com", getXPCOMStatusFromNSS(122), true);
  // SEC_ERROR_OCSP_REQUEST_NEEDS_SIG = (SEC_ERROR_BASE + 123)
  add_connection_test("ocsp-stapling-needssig.example.com", getXPCOMStatusFromNSS(123), true);
  // SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST = (SEC_ERROR_BASE + 124)
  add_connection_test("ocsp-stapling-unauthorized.example.com", getXPCOMStatusFromNSS(124), true);
  // SEC_ERROR_OCSP_UNKNOWN_CERT = (SEC_ERROR_BASE + 126)
  add_connection_test("ocsp-stapling-unknown.example.com", getXPCOMStatusFromNSS(126), true);
  add_connection_test("ocsp-stapling-good-other.example.com", getXPCOMStatusFromNSS(126), true);
  // SEC_ERROR_OCSP_MALFORMED_RESPONSE = (SEC_ERROR_BASE + 129)
  add_connection_test("ocsp-stapling-none.example.com", getXPCOMStatusFromNSS(129), true);
  // SEC_ERROR_OCSP_OLD_RESPONSE = (SEC_ERROR_BASE + 132)
  add_connection_test("ocsp-stapling-expired.example.com", getXPCOMStatusFromNSS(132), true);
  add_connection_test("ocsp-stapling-expired-fresh-ca.example.com", getXPCOMStatusFromNSS(132), true);
  do_register_cleanup(function() { gHttpServer.stop(cleanup); });
  run_next_test();
  do_test_finished();
}
