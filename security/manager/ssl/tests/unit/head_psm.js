/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

const { 'classes': Cc, 'interfaces': Ci, 'utils': Cu, 'results': Cr } = Components;

let { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm", {});
let { FileUtils } = Cu.import("resource://gre/modules/FileUtils.jsm", {});
let { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
let { Promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
let { HttpServer } = Cu.import("resource://testing-common/httpd.js", {});
let { ctypes } = Cu.import("resource://gre/modules/ctypes.jsm");

let gIsWindows = ("@mozilla.org/windows-registry-key;1" in Cc);

const isDebugBuild = Cc["@mozilla.org/xpcom/debug;1"]
                       .getService(Ci.nsIDebug2).isDebugBuild;

const SSS_STATE_FILE_NAME = "SiteSecurityServiceState.txt";

const SEC_ERROR_BASE = Ci.nsINSSErrorsService.NSS_SEC_ERROR_BASE;
const SSL_ERROR_BASE = Ci.nsINSSErrorsService.NSS_SSL_ERROR_BASE;
const MOZILLA_PKIX_ERROR_BASE = Ci.nsINSSErrorsService.MOZILLA_PKIX_ERROR_BASE;

// Sort in numerical order
const SEC_ERROR_INVALID_ARGS                            = SEC_ERROR_BASE +   5; // -8187
const SEC_ERROR_BAD_DER                                 = SEC_ERROR_BASE +   9;
const SEC_ERROR_EXPIRED_CERTIFICATE                     = SEC_ERROR_BASE +  11;
const SEC_ERROR_REVOKED_CERTIFICATE                     = SEC_ERROR_BASE +  12; // -8180
const SEC_ERROR_UNKNOWN_ISSUER                          = SEC_ERROR_BASE +  13;
const SEC_ERROR_BAD_DATABASE                            = SEC_ERROR_BASE +  18;
const SEC_ERROR_UNTRUSTED_ISSUER                        = SEC_ERROR_BASE +  20; // -8172
const SEC_ERROR_UNTRUSTED_CERT                          = SEC_ERROR_BASE +  21; // -8171
const SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE              = SEC_ERROR_BASE +  30; // -8162
const SEC_ERROR_EXTENSION_VALUE_INVALID                 = SEC_ERROR_BASE +  34; // -8158
const SEC_ERROR_EXTENSION_NOT_FOUND                     = SEC_ERROR_BASE +  35; // -8157
const SEC_ERROR_CA_CERT_INVALID                         = SEC_ERROR_BASE +  36;
const SEC_ERROR_INVALID_KEY                             = SEC_ERROR_BASE +  40; // -8152
const SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION              = SEC_ERROR_BASE +  41;
const SEC_ERROR_INADEQUATE_KEY_USAGE                    = SEC_ERROR_BASE +  90; // -8102
const SEC_ERROR_INADEQUATE_CERT_TYPE                    = SEC_ERROR_BASE +  91; // -8101
const SEC_ERROR_CERT_NOT_IN_NAME_SPACE                  = SEC_ERROR_BASE + 112; // -8080
const SEC_ERROR_CERT_BAD_ACCESS_LOCATION                = SEC_ERROR_BASE + 117; // -8075
const SEC_ERROR_OCSP_MALFORMED_REQUEST                  = SEC_ERROR_BASE + 120;
const SEC_ERROR_OCSP_SERVER_ERROR                       = SEC_ERROR_BASE + 121; // -8071
const SEC_ERROR_OCSP_TRY_SERVER_LATER                   = SEC_ERROR_BASE + 122;
const SEC_ERROR_OCSP_REQUEST_NEEDS_SIG                  = SEC_ERROR_BASE + 123;
const SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST               = SEC_ERROR_BASE + 124;
const SEC_ERROR_OCSP_UNKNOWN_CERT                       = SEC_ERROR_BASE + 126; // -8066
const SEC_ERROR_OCSP_MALFORMED_RESPONSE                 = SEC_ERROR_BASE + 129;
const SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE              = SEC_ERROR_BASE + 130;
const SEC_ERROR_OCSP_OLD_RESPONSE                       = SEC_ERROR_BASE + 132;
const SEC_ERROR_OCSP_INVALID_SIGNING_CERT               = SEC_ERROR_BASE + 144;
const SEC_ERROR_POLICY_VALIDATION_FAILED                = SEC_ERROR_BASE + 160; // -8032
const SEC_ERROR_OCSP_BAD_SIGNATURE                      = SEC_ERROR_BASE + 157;
const SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED       = SEC_ERROR_BASE + 176;
const SEC_ERROR_APPLICATION_CALLBACK_ERROR              = SEC_ERROR_BASE + 178;

const SSL_ERROR_BAD_CERT_DOMAIN                         = SSL_ERROR_BASE +  12;
const SSL_ERROR_BAD_CERT_ALERT                          = SSL_ERROR_BASE +  17;

const MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE            = MOZILLA_PKIX_ERROR_BASE +   0;
const MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY     = MOZILLA_PKIX_ERROR_BASE +   1;
const MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE            = MOZILLA_PKIX_ERROR_BASE +   2; // -16382

// Supported Certificate Usages
const certificateUsageSSLClient              = 0x0001;
const certificateUsageSSLServer              = 0x0002;
const certificateUsageSSLCA                  = 0x0008;
const certificateUsageEmailSigner            = 0x0010;
const certificateUsageEmailRecipient         = 0x0020;
const certificateUsageObjectSigner           = 0x0040;
const certificateUsageVerifyCA               = 0x0100;
const certificateUsageStatusResponder        = 0x0400;

const NO_FLAGS = 0;

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

function constructCertFromFile(filename) {
  let certFile = do_get_file(filename, false);
  let certDER = readFile(certFile);
  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);
  return certdb.constructX509(certDER, certDER.length);
}

function setCertTrust(cert, trustString) {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);
  certdb.setCertTrustFromString(cert, trustString);
}

function getXPCOMStatusFromNSS(statusNSS) {
  let nssErrorsService = Cc["@mozilla.org/nss_errors_service;1"]
                           .getService(Ci.nsINSSErrorsService);
  return nssErrorsService.getXPCOMFromNSSError(statusNSS);
}

function checkCertErrorGeneric(certdb, cert, expectedError, usage) {
  let hasEVPolicy = {};
  let verifiedChain = {};
  let error = certdb.verifyCertNow(cert, usage, NO_FLAGS, verifiedChain,
                                   hasEVPolicy);
  // expected error == -1 is a special marker for any error is OK
  if (expectedError != -1 ) {
    do_check_eq(error, expectedError);
  } else {
    do_check_neq (error, 0);
  }
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
  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);
  certdb.clearOCSPCache();
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

  add_connection_test("<test-name-1>.example.com",
                      getXPCOMStatusFromNSS(SEC_ERROR_xxx),
                      function() { ... },
                      function(aTransportSecurityInfo) { ... },
                      function(aTransport) { ... });
  [...]
  add_connection_test("<test-name-n>.example.com", Cr.NS_OK);

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
// aAfterStreamOpen is a callback function that is called with the
// nsISocketTransport once the output stream is ready.
function add_connection_test(aHost, aExpectedResult,
                             aBeforeConnect, aWithSecurityInfo,
                             aAfterStreamOpen) {
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
      if (aAfterStreamOpen) {
        aAfterStreamOpen(this.transport);
      }
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
      do_print("handling " + aHost);
      do_check_eq(conn.result, aExpectedResult);
      if (aWithSecurityInfo) {
        aWithSecurityInfo(conn.transport.securityInfo
                              .QueryInterface(Ci.nsITransportSecurityInfo));
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
  // But maybe we're on Android or B2G, where binaries are in /data/local/xpcb.
  if (!utilBin.exists()) {
    utilBin.initWithPath("/data/local/xpcb/");
    utilBin.append(binaryUtilName);
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
  // Using "sql:" causes the SQL DB to be used so we can run tests on Android.
  process.run(false, [ "sql:" + certDir.path ], 1);

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
    // Using "sql:" causes the SQL DB to be used so we can run tests on Android.
    argArray.push("sql:" + nssDBlocation);
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

// Starts and returns an http responder that will cause a test failure if it is
// queried. The server identities are given by a non-empty array
// serverIdentities.
function getFailingHttpServer(serverPort, serverIdentities) {
  let httpServer = new HttpServer();
  httpServer.registerPrefixHandler("/", function(request, response) {
    do_check_true(false);
  });
  httpServer.identity.setPrimary("http", serverIdentities.shift(), serverPort);
  serverIdentities.forEach(function(identity) {
    httpServer.identity.add("http", identity, serverPort);
  });
  httpServer.start(serverPort);
  return httpServer;
}

// Starts an http OCSP responder that serves good OCSP responses and
// returns an object with a method stop that should be called to stop
// the http server.
// NB: Because generating OCSP responses inside the HTTP request
// handler can cause timeouts, the expected responses are pre-generated
// all at once before starting the server. This means that their producedAt
// times will all be the same. If a test depends on this not being the case,
// perhaps calling startOCSPResponder twice (at different times) will be
// necessary.
//
// serverPort is the port of the http OCSP responder
// identity is the http hostname that will answer the OCSP requests
// invalidIdentities is an array of identities that if used an
//   will cause a test failure
// nssDBlocaion is the location of the NSS database from where the OCSP
//   responses will be generated (assumes appropiate keys are present)
// expectedCertNames is an array of nicks of the certs to be responsed
// expectedBasePaths is an optional array that is used to indicate
//   what is the expected base path of the OCSP request.
function startOCSPResponder(serverPort, identity, invalidIdentities,
                            nssDBLocation, expectedCertNames,
                            expectedBasePaths, expectedMethods,
                            expectedResponseTypes) {
  let ocspResponseGenerationArgs = expectedCertNames.map(
    function(expectedNick) {
      let responseType = "good";
      if (expectedResponseTypes && expectedResponseTypes.length >= 1) {
        responseType = expectedResponseTypes.shift();
      }
      return [responseType, expectedNick, "unused"];
    }
  );
  let ocspResponses = generateOCSPResponses(ocspResponseGenerationArgs,
                                            nssDBLocation);
  let httpServer = new HttpServer();
  httpServer.registerPrefixHandler("/",
    function handleServerCallback(aRequest, aResponse) {
      invalidIdentities.forEach(function(identity) {
        do_check_neq(aRequest.host, identity)
      });
      do_print("got request for: " + aRequest.path);
      let basePath = aRequest.path.slice(1).split("/")[0];
      if (expectedBasePaths.length >= 1) {
        do_check_eq(basePath, expectedBasePaths.shift());
      }
      do_check_true(expectedCertNames.length >= 1);
      if (expectedMethods && expectedMethods.length >= 1) {
        do_check_eq(aRequest.method, expectedMethods.shift());
      }
      aResponse.setStatusLine(aRequest.httpVersion, 200, "OK");
      aResponse.setHeader("Content-Type", "application/ocsp-response");
      aResponse.write(ocspResponses.shift());
    });
  httpServer.identity.setPrimary("http", identity, serverPort);
  invalidIdentities.forEach(function(identity) {
    httpServer.identity.add("http", identity, serverPort);
  });
  httpServer.start(serverPort);
  return {
    stop: function(callback) {
      // make sure we consumed each expected response
      do_check_eq(ocspResponses.length, 0);
      if (expectedMethods) {
        do_check_eq(expectedMethods.length, 0);
      }
      if (expectedBasePaths) {
        do_check_eq(expectedBasePaths.length, 0);
      }
      if (expectedResponseTypes) {
        do_check_eq(expectedResponseTypes.length, 0);
      }
      httpServer.stop(callback);
    }
  };
}

// A prototype for a fake, error-free sslstatus
let FakeSSLStatus = function(certificate) {
  this.serverCert = certificate;
};

FakeSSLStatus.prototype = {
  serverCert: null,
  cipherName: null,
  keyLength: 2048,
  isDomainMismatch: false,
  isNotValidAtThisTime: false,
  isUntrusted: false,
  isExtendedValidation: false,
  getInterface: function(aIID) {
    return this.QueryInterface(aIID);
  },
  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsISSLStatus) ||
        aIID.equals(Ci.nsISupports)) {
      return this;
    }
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
}
