/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

const { 'classes': Cc, 'interfaces': Ci, 'utils': Cu, 'results': Cr } = Components;

let { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm", {});
let { FileUtils } = Cu.import("resource://gre/modules/FileUtils.jsm", {});
let { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
let { Promise } = Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js", {});
let { HttpServer } = Cu.import("resource://testing-common/httpd.js", {});
let { ctypes } = Cu.import("resource://gre/modules/ctypes.jsm");

let gIsWindows = ("@mozilla.org/windows-registry-key;1" in Cc);

const SEC_ERROR_BASE = Ci.nsINSSErrorsService.NSS_SEC_ERROR_BASE;

// Sort in numerical order
const SEC_ERROR_REVOKED_CERTIFICATE                     = SEC_ERROR_BASE +  12;
const SEC_ERROR_BAD_DATABASE                            = SEC_ERROR_BASE +  18;
const SEC_ERROR_OCSP_MALFORMED_REQUEST                  = SEC_ERROR_BASE + 120;
const SEC_ERROR_OCSP_SERVER_ERROR                       = SEC_ERROR_BASE + 121;
const SEC_ERROR_OCSP_TRY_SERVER_LATER                   = SEC_ERROR_BASE + 122;
const SEC_ERROR_OCSP_REQUEST_NEEDS_SIG                  = SEC_ERROR_BASE + 123;
const SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST               = SEC_ERROR_BASE + 124;
const SEC_ERROR_OCSP_UNKNOWN_CERT                       = SEC_ERROR_BASE + 126;
const SEC_ERROR_OCSP_MALFORMED_RESPONSE                 = SEC_ERROR_BASE + 129;
const SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE              = SEC_ERROR_BASE + 130;
const SEC_ERROR_OCSP_OLD_RESPONSE                       = SEC_ERROR_BASE + 132;
const SEC_ERROR_OCSP_INVALID_SIGNING_CERT               = SEC_ERROR_BASE + 144;

// Certificate Usages
const certificateUsageSSLClient              = 0x0001;
const certificateUsageSSLServer              = 0x0002;
const certificateUsageSSLServerWithStepUp    = 0x0004;
const certificateUsageSSLCA                  = 0x0008;
const certificateUsageEmailSigner            = 0x0010;
const certificateUsageEmailRecipient         = 0x0020;
const certificateUsageObjectSigner           = 0x0040;
const certificateUsageUserCertImport         = 0x0080;
const certificateUsageVerifyCA               = 0x0100;
const certificateUsageProtectedObjectSigner  = 0x0200;
const certificateUsageStatusResponder        = 0x0400;
const certificateUsageAnyCA                  = 0x0800;

function readFile(file) {
  let fstream = Cc["@mozilla.org/network/file-input-stream;1"]
                  .createInstance(Ci.nsIFileInputStream);
  fstream.init(file, -1, 0, 0);
  let data = NetUtil.readInputStreamToString(fstream, fstream.available());
  fstream.close();
  return data;
}

function addCertFromFile(certdb, filename, trustString) {
  let certFile = do_get_file(filename, false);
  let der = readFile(certFile);
  certdb.addCert(der, trustString, null);
}

function getXPCOMStatusFromNSS(statusNSS) {
  let nssErrorsService = Cc["@mozilla.org/nss_errors_service;1"]
                           .getService(Ci.nsINSSErrorsService);
  return nssErrorsService.getXPCOMFromNSSError(statusNSS);
}

function _getLibraryFunctionWithNoArguments(functionName, libraryName) {
  // Open the NSS library. copied from services/crypto/modules/WeaveCrypto.js
  let path = ctypes.libraryName(libraryName);

  // XXX really want to be able to pass specific dlopen flags here.
  let nsslib;
  try {
    nsslib = ctypes.open(path);
  } catch(e) {
    // In case opening the library without a full path fails,
    // try again with a full path.
    let file = Services.dirsvc.get("GreD", Ci.nsILocalFile);
    file.append(path);
    nsslib = ctypes.open(file.path);
  }

  let SECStatus = ctypes.int;
  let func = nsslib.declare(functionName, ctypes.default_abi, SECStatus);
  return func;
}

function clearOCSPCache() {
  let CERT_ClearOCSPCache =
    _getLibraryFunctionWithNoArguments("CERT_ClearOCSPCache", "nss3");
  if (CERT_ClearOCSPCache() != 0) {
    throw "Failed to clear OCSP cache";
  }
}

function clearSessionCache() {
  let SSL_ClearSessionCache = null;
  try {
    SSL_ClearSessionCache =
      _getLibraryFunctionWithNoArguments("SSL_ClearSessionCache", "ssl3");
  } catch (e) {
    // On Windows, this is actually in the nss3 library.
    SSL_ClearSessionCache =
      _getLibraryFunctionWithNoArguments("SSL_ClearSessionCache", "nss3");
  }
  if (!SSL_ClearSessionCache || SSL_ClearSessionCache() != 0) {
    throw "Failed to clear SSL session cache";
  }
}

// Set up a TLS testing environment that has a TLS server running and
// ready to accept connections. This async function starts the server and
// waits for the server to indicate that it is ready.
//
// Each test should have its own subdomain of example.com, for example
// my-first-connection-test.example.com. The server can use the server
// name (passed through the SNI TLS extension) to determine what behavior
// the server side of the text should exhibit. See TLSServer.h for more
// information on how to write the server side of tests.
//
// Create a new source file for your new server executable in
// security/manager/ssl/tests/unit/tlsserver/cmd similar to the other ones in
// that directory, and add a reference to it to the sources variable in that
// directory's moz.build.
//
// Modify TEST_HARNESS_BINS in
// testing/mochitest/Makefile.in and NO_PKG_FILES in
// toolkit/mozapps/installer/packager.mk to make sure the new executable
// gets included in the packages used for shipping the tests to the test
// runners in our build/test farm. (Things will work fine locally without
// these changes but will break on TBPL.)
//
// Your test script should look something like this:
/*

// -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// <documentation on your test>

function run_test() {
  do_get_profile();
  add_tls_server_setup("<test-server-name>");

  add_connection_test("<test-name-1>.example.com", Cr.<expected result>,
                      <ocsp stapling enabled>);
  [...]
  add_connection_test("<test-name-n>.example.com", Cr.<expected result>,
                      <ocsp stapling enabled>);

  run_next_test();
}

*/
function add_tls_server_setup(serverBinName) {
  add_test(function() {
    _setupTLSServerTest(serverBinName);
  });
}

// Add a TLS connection test case. aHost is the hostname to pass in the SNI TLS
// extension; this should unambiguously identifiy which test is being run.
// aExpectedResult is the expected nsresult of the connection.
// aBeforeConnect is a callback function that takes no arguments that will be
// called before the connection is attempted.
// aWithSecurityInfo is a callback function that takes an
// nsITransportSecurityInfo, which is called after the TLS handshake succeeds.
function add_connection_test(aHost, aExpectedResult,
                             aBeforeConnect, aWithSecurityInfo) {
  const REMOTE_PORT = 8443;

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
        this.outputStream.close();
        this.result = Cr.NS_OK;
      } catch (e) {
        this.result = e.result;
      }
      this.defer.resolve(this);
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

  add_test(function() {
    if (aBeforeConnect) {
      aBeforeConnect();
    }
    connectTo(aHost).then(function(conn) {
      dump("hello #0\n");
      do_check_eq(conn.result, aExpectedResult);
      dump("hello #0.5\n");
      if (aWithSecurityInfo) {
        dump("hello #1\n");
        aWithSecurityInfo(conn.transport.securityInfo
                              .QueryInterface(Ci.nsITransportSecurityInfo));
        dump("hello #2\n");
      }
      run_next_test();
    });
  });
}

function _getBinaryUtil(binaryUtilName) {
  let directoryService = Cc["@mozilla.org/file/directory_service;1"]
                           .getService(Ci.nsIProperties);

  let utilBin = directoryService.get("CurProcD", Ci.nsILocalFile);
  utilBin.append(binaryUtilName + (gIsWindows ? ".exe" : ""));
  // If we're testing locally, the above works. If not, the server executable
  // is in another location.
  if (!utilBin.exists()) {
    utilBin = directoryService.get("CurWorkD", Ci.nsILocalFile);
    while (utilBin.path.indexOf("xpcshell") != -1) {
      utilBin = utilBin.parent;
    }
    utilBin.append("bin");
    utilBin.append(binaryUtilName + (gIsWindows ? ".exe" : ""));
  }
  do_check_true(utilBin.exists());
  return utilBin;
}

// Do not call this directly; use add_tls_server_setup
function _setupTLSServerTest(serverBinName)
{
  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);
  // The trusted CA that is typically used for "good" certificates.
  addCertFromFile(certdb, "tlsserver/test-ca.der", "CTu,u,u");

  const CALLBACK_PORT = 8444;

  let directoryService = Cc["@mozilla.org/file/directory_service;1"]
                           .getService(Ci.nsIProperties);
  let envSvc = Cc["@mozilla.org/process/environment;1"]
                 .getService(Ci.nsIEnvironment);
  let greDir = directoryService.get("GreD", Ci.nsIFile);
  envSvc.set("DYLD_LIBRARY_PATH", greDir.path);
  envSvc.set("LD_LIBRARY_PATH", greDir.path);
  envSvc.set("MOZ_TLS_SERVER_DEBUG_LEVEL", "3");
  envSvc.set("MOZ_TLS_SERVER_CALLBACK_PORT", CALLBACK_PORT);

  let httpServer = new HttpServer();
  httpServer.registerPathHandler("/",
      function handleServerCallback(aRequest, aResponse) {
        aResponse.setStatusLine(aRequest.httpVersion, 200, "OK");
        aResponse.setHeader("Content-Type", "text/plain");
        let responseBody = "OK!";
        aResponse.bodyOutputStream.write(responseBody, responseBody.length);
        do_execute_soon(function() {
          httpServer.stop(run_next_test);
        });
      });
  httpServer.start(CALLBACK_PORT);

  let serverBin = _getBinaryUtil(serverBinName);
  let process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
  process.init(serverBin);
  let certDir = directoryService.get("CurWorkD", Ci.nsILocalFile);
  certDir.append("tlsserver");
  do_check_true(certDir.exists());
  process.run(false, [certDir.path], 1);

  do_register_cleanup(function() {
    process.kill();
  });
}

// Returns an Array of OCSP responses for a given ocspRespArray and a location
// for a nssDB where the certs and public keys are prepopulated.
// ocspRespArray is an array of arrays like:
// [ [typeOfResponse, certnick, extracertnick]...]
function generateOCSPResponses(ocspRespArray, nssDBlocation)
{
  let utilBinName =  "GenerateOCSPResponse";
  let ocspGenBin = _getBinaryUtil(utilBinName);
  let retArray = new Array();

  for (let i = 0; i < ocspRespArray.length; i++) {
    let argArray = new Array();
    let ocspFilepre = do_get_file(i.toString() + ".ocsp", true);
    let filename = ocspFilepre.path;
    argArray.push(nssDBlocation);
    argArray.push(ocspRespArray[i][0]); // ocsRespType;
    argArray.push(ocspRespArray[i][1]); // nick;
    argArray.push(ocspRespArray[i][2]); // extranickname
    argArray.push(filename);
    do_print("arg_array ="+argArray);

    let process = Cc["@mozilla.org/process/util;1"]
                    .createInstance(Ci.nsIProcess);
    process.init(ocspGenBin);
    process.run(true, argArray, 5);
    do_check_eq(0, process.exitValue);
    let ocspFile = do_get_file(i.toString() + ".ocsp", false);
    retArray.push(readFile(ocspFile));
    ocspFile.remove(false);
  }
  return retArray;
}
