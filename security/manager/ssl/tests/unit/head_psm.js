/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

const { AppConstants } =
  Cu.import("resource://gre/modules/AppConstants.jsm", {});
const { ctypes } = Cu.import("resource://gre/modules/ctypes.jsm", {});
const { FileUtils } = Cu.import("resource://gre/modules/FileUtils.jsm", {});
const { HttpServer } = Cu.import("resource://testing-common/httpd.js", {});
const { MockRegistrar } =
  Cu.import("resource://testing-common/MockRegistrar.jsm", {});
const { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm", {});
const { Promise } = Cu.import("resource://gre/modules/Promise.jsm", {});
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});
const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});

const isDebugBuild = Cc["@mozilla.org/xpcom/debug;1"]
                       .getService(Ci.nsIDebug2).isDebugBuild;

// The test EV roots are only enabled in debug builds as a security measure.
//
// Bug 1008316: B2G doesn't have EV enabled, so EV is not expected even in debug
// builds.
const gEVExpected = isDebugBuild && !("@mozilla.org/b2g-process-global;1" in Cc);

const SSS_STATE_FILE_NAME = "SiteSecurityServiceState.txt";
const PRELOAD_STATE_FILE_NAME = "SecurityPreloadState.txt";

const SEC_ERROR_BASE = Ci.nsINSSErrorsService.NSS_SEC_ERROR_BASE;
const SSL_ERROR_BASE = Ci.nsINSSErrorsService.NSS_SSL_ERROR_BASE;
const MOZILLA_PKIX_ERROR_BASE = Ci.nsINSSErrorsService.MOZILLA_PKIX_ERROR_BASE;

// This isn't really a valid PRErrorCode, but is useful for signalling that
// a test is expected to succeed.
const PRErrorCodeSuccess = 0;

// Sort in numerical order
const SEC_ERROR_INVALID_TIME                            = SEC_ERROR_BASE + 8;
const SEC_ERROR_BAD_DER                                 = SEC_ERROR_BASE + 9;
const SEC_ERROR_BAD_SIGNATURE                           = SEC_ERROR_BASE + 10;
const SEC_ERROR_EXPIRED_CERTIFICATE                     = SEC_ERROR_BASE + 11;
const SEC_ERROR_REVOKED_CERTIFICATE                     = SEC_ERROR_BASE + 12;
const SEC_ERROR_UNKNOWN_ISSUER                          = SEC_ERROR_BASE + 13;
const SEC_ERROR_UNTRUSTED_ISSUER                        = SEC_ERROR_BASE + 20;
const SEC_ERROR_UNTRUSTED_CERT                          = SEC_ERROR_BASE + 21;
const SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE              = SEC_ERROR_BASE + 30;
const SEC_ERROR_CA_CERT_INVALID                         = SEC_ERROR_BASE + 36;
const SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION              = SEC_ERROR_BASE + 41;
const SEC_ERROR_PKCS7_BAD_SIGNATURE                     = SEC_ERROR_BASE + 47;
const SEC_ERROR_INADEQUATE_KEY_USAGE                    = SEC_ERROR_BASE + 90;
const SEC_ERROR_INADEQUATE_CERT_TYPE                    = SEC_ERROR_BASE + 91;
const SEC_ERROR_CERT_NOT_IN_NAME_SPACE                  = SEC_ERROR_BASE + 112;
const SEC_ERROR_CERT_BAD_ACCESS_LOCATION                = SEC_ERROR_BASE + 117;
const SEC_ERROR_OCSP_MALFORMED_REQUEST                  = SEC_ERROR_BASE + 120;
const SEC_ERROR_OCSP_SERVER_ERROR                       = SEC_ERROR_BASE + 121;
const SEC_ERROR_OCSP_TRY_SERVER_LATER                   = SEC_ERROR_BASE + 122;
const SEC_ERROR_OCSP_REQUEST_NEEDS_SIG                  = SEC_ERROR_BASE + 123;
const SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST               = SEC_ERROR_BASE + 124;
const SEC_ERROR_OCSP_UNKNOWN_CERT                       = SEC_ERROR_BASE + 126;
const SEC_ERROR_OCSP_MALFORMED_RESPONSE                 = SEC_ERROR_BASE + 129;
const SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE              = SEC_ERROR_BASE + 130;
const SEC_ERROR_OCSP_OLD_RESPONSE                       = SEC_ERROR_BASE + 132;
const SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE              = SEC_ERROR_BASE + 141;
const SEC_ERROR_OCSP_INVALID_SIGNING_CERT               = SEC_ERROR_BASE + 144;
const SEC_ERROR_POLICY_VALIDATION_FAILED                = SEC_ERROR_BASE + 160;
const SEC_ERROR_OCSP_BAD_SIGNATURE                      = SEC_ERROR_BASE + 157;
const SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED       = SEC_ERROR_BASE + 176;

const SSL_ERROR_NO_CYPHER_OVERLAP                       = SSL_ERROR_BASE + 2;
const SSL_ERROR_BAD_CERT_DOMAIN                         = SSL_ERROR_BASE + 12;
const SSL_ERROR_BAD_CERT_ALERT                          = SSL_ERROR_BASE + 17;
const SSL_ERROR_WEAK_SERVER_CERT_KEY                    = SSL_ERROR_BASE + 132;

const MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE            = MOZILLA_PKIX_ERROR_BASE + 0;
const MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY     = MOZILLA_PKIX_ERROR_BASE + 1;
const MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE            = MOZILLA_PKIX_ERROR_BASE + 2;
const MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA             = MOZILLA_PKIX_ERROR_BASE + 3;
const MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE      = MOZILLA_PKIX_ERROR_BASE + 5;
const MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE = MOZILLA_PKIX_ERROR_BASE + 6;
const MOZILLA_PKIX_ERROR_OCSP_RESPONSE_FOR_CERT_MISSING = MOZILLA_PKIX_ERROR_BASE + 8;
const MOZILLA_PKIX_ERROR_REQUIRED_TLS_FEATURE_MISSING   = MOZILLA_PKIX_ERROR_BASE + 10;
const MOZILLA_PKIX_ERROR_EMPTY_ISSUER_NAME              = MOZILLA_PKIX_ERROR_BASE + 12;

// Supported Certificate Usages
const certificateUsageSSLClient              = 0x0001;
const certificateUsageSSLServer              = 0x0002;
const certificateUsageSSLCA                  = 0x0008;
const certificateUsageEmailSigner            = 0x0010;
const certificateUsageEmailRecipient         = 0x0020;

// A map from the name of a certificate usage to the value of the usage.
// Useful for printing debugging information and for enumerating all supported
// usages.
const allCertificateUsages = {
  certificateUsageSSLClient,
  certificateUsageSSLServer,
  certificateUsageSSLCA,
  certificateUsageEmailSigner,
  certificateUsageEmailRecipient,
};

const NO_FLAGS = 0;

// Commonly certificates are represented as PEM. The format is roughly as
// follows:
//
// -----BEGIN CERTIFICATE-----
// [some lines of base64, each typically 64 characters long]
// -----END CERTIFICATE-----
//
// However, nsIX509CertDB.constructX509FromBase64 and related functions do not
// handle input of this form. Instead, they require a single string of base64
// with no newlines or BEGIN/END headers. This is a helper function to convert
// PEM to the format that nsIX509CertDB requires.
function pemToBase64(pem) {
  return pem.replace(/-----BEGIN CERTIFICATE-----/, "")
            .replace(/-----END CERTIFICATE-----/, "")
            .replace(/[\r\n]/g, "");
}

function build_cert_chain(certNames) {
  let certList = Cc["@mozilla.org/security/x509certlist;1"]
                   .createInstance(Ci.nsIX509CertList);
  certNames.forEach(function(certName) {
    let cert = constructCertFromFile("bad_certs/" + certName + ".pem");
    certList.addCert(cert);
  });
  return certList;
}

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
  let certBytes = readFile(certFile);
  try {
    return certdb.addCert(certBytes, trustString);
  } catch (e) {}
  // It might be PEM instead of DER.
  return certdb.addCertFromBase64(pemToBase64(certBytes), trustString);
}

function constructCertFromFile(filename) {
  let certFile = do_get_file(filename, false);
  let certBytes = readFile(certFile);
  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);
  try {
    return certdb.constructX509(certBytes);
  } catch (e) {}
  // It might be PEM instead of DER.
  return certdb.constructX509FromBase64(pemToBase64(certBytes));
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

// certdb implements nsIX509CertDB. See nsIX509CertDB.idl for documentation.
// In particular, hostname is optional.
function checkCertErrorGenericAtTime(certdb, cert, expectedError, usage, time,
                                     /* optional */ hasEVPolicy,
                                     /* optional */ hostname) {
  info(`cert cn=${cert.commonName}`);
  info(`cert issuer cn=${cert.issuerCommonName}`);
  let verifiedChain = {};
  let error = certdb.verifyCertAtTime(cert, usage, NO_FLAGS, hostname, time,
                                      verifiedChain, hasEVPolicy || {});
  Assert.equal(error, expectedError,
               "Actual and expected error should match");
}

// certdb implements nsIX509CertDB. See nsIX509CertDB.idl for documentation.
// In particular, hostname is optional.
function checkCertErrorGeneric(certdb, cert, expectedError, usage,
                               /* optional */ hasEVPolicy,
                               /* optional */ hostname) {
  info(`cert cn=${cert.commonName}`);
  info(`cert issuer cn=${cert.issuerCommonName}`);
  let verifiedChain = {};
  let error = certdb.verifyCertNow(cert, usage, NO_FLAGS, hostname,
                                   verifiedChain, hasEVPolicy || {});
  Assert.equal(error, expectedError,
               "Actual and expected error should match");
}

function checkEVStatus(certDB, cert, usage, isEVExpected) {
  let hasEVPolicy = {};
  checkCertErrorGeneric(certDB, cert, PRErrorCodeSuccess, usage, hasEVPolicy);
  Assert.equal(hasEVPolicy.value, isEVExpected,
               "Actual and expected EV status should match");
}

function _getLibraryFunctionWithNoArguments(functionName, libraryName,
                                            returnType) {
  // Open the NSS library. copied from services/crypto/modules/WeaveCrypto.js
  let path = ctypes.libraryName(libraryName);

  // XXX really want to be able to pass specific dlopen flags here.
  let nsslib;
  try {
    nsslib = ctypes.open(path);
  } catch (e) {
    // In case opening the library without a full path fails,
    // try again with a full path.
    let file = Services.dirsvc.get("GreBinD", Ci.nsIFile);
    file.append(path);
    nsslib = ctypes.open(file.path);
  }

  let SECStatus = ctypes.int;
  let func = nsslib.declare(functionName, ctypes.default_abi,
                            returnType || SECStatus);
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
      _getLibraryFunctionWithNoArguments("SSL_ClearSessionCache", "ssl3",
                                         ctypes.void_t);
  } catch (e) {
    // On Windows, this is actually in the nss3 library.
    SSL_ClearSessionCache =
      _getLibraryFunctionWithNoArguments("SSL_ClearSessionCache", "nss3",
                                         ctypes.void_t);
  }
  if (!SSL_ClearSessionCache) {
    throw new Error("couldn't get SSL_ClearSessionCache");
  }
  SSL_ClearSessionCache();
}

function getSSLStatistics() {
  let SSL3Statistics = new ctypes.StructType("SSL3Statistics",
                           [ { "sch_sid_cache_hits": ctypes.long },
                             { "sch_sid_cache_misses": ctypes.long },
                             { "sch_sid_cache_not_ok": ctypes.long },
                             { "hsh_sid_cache_hits": ctypes.long },
                             { "hsh_sid_cache_misses": ctypes.long },
                             { "hsh_sid_cache_not_ok": ctypes.long },
                             { "hch_sid_cache_hits": ctypes.long },
                             { "hch_sid_cache_misses": ctypes.long },
                             { "hch_sid_cache_not_ok": ctypes.long },
                             { "sch_sid_stateless_resumes": ctypes.long },
                             { "hsh_sid_stateless_resumes": ctypes.long },
                             { "hch_sid_stateless_resumes": ctypes.long },
                             { "hch_sid_ticket_parse_failures": ctypes.long }]);
  let SSL3StatisticsPtr = new ctypes.PointerType(SSL3Statistics);
  let SSL_GetStatistics = null;
  try {
    SSL_GetStatistics = _getLibraryFunctionWithNoArguments("SSL_GetStatistics",
                                                           "ssl3",
                                                           SSL3StatisticsPtr);
  } catch (e) {
    // On Windows, this is actually in the nss3 library.
    SSL_GetStatistics = _getLibraryFunctionWithNoArguments("SSL_GetStatistics",
                                                           "nss3",
                                                           SSL3StatisticsPtr);
  }
  if (!SSL_GetStatistics) {
    throw new Error("Failed to get SSL statistics");
  }
  return SSL_GetStatistics();
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
  add_tls_server_setup("<test-server-name>", "<path-to-certificate-directory>");

  add_connection_test("<test-name-1>.example.com",
                      SEC_ERROR_xxx,
                      function() { ... },
                      function(aTransportSecurityInfo) { ... },
                      function(aTransport) { ... });
  [...]
  add_connection_test("<test-name-n>.example.com", PRErrorCodeSuccess);

  run_next_test();
}
*/

function add_tls_server_setup(serverBinName, certsPath, addDefaultRoot = true) {
  add_test(function() {
    _setupTLSServerTest(serverBinName, certsPath, addDefaultRoot);
  });
}

/**
 * Add a TLS connection test case.
 *
 * @param {String} aHost
 *   The hostname to pass in the SNI TLS extension; this should unambiguously
 *   identify which test is being run.
 * @param {PRErrorCode} aExpectedResult
 *   The expected result of the connection. If an error is not expected, pass
 *   in PRErrorCodeSuccess.
 * @param {Function} aBeforeConnect
 *   A callback function that takes no arguments that will be called before the
 *   connection is attempted.
 * @param {Function} aWithSecurityInfo
 *   A callback function that takes an nsITransportSecurityInfo, which is called
 *   after the TLS handshake succeeds.
 * @param {Function} aAfterStreamOpen
 *   A callback function that is called with the nsISocketTransport once the
 *   output stream is ready.
 * @param {OriginAttributes} aOriginAttributes (optional)
 *   The origin attributes that the socket transport will have. This parameter
 *   affects OCSP because OCSP cache is double-keyed by origin attributes' first
 *   party domain.
 */
function add_connection_test(aHost, aExpectedResult,
                             aBeforeConnect, aWithSecurityInfo,
                             aAfterStreamOpen,
                             /* optional */ aOriginAttributes) {
  const REMOTE_PORT = 8443;

  function Connection(host) {
    this.host = host;
    this.thread = Services.tm.currentThread;
    this.defer = Promise.defer();
    let sts = Cc["@mozilla.org/network/socket-transport-service;1"]
                .getService(Ci.nsISocketTransportService);
    this.transport = sts.createTransport(["ssl"], 1, host, REMOTE_PORT, null);
    // See bug 1129771 - attempting to connect to [::1] when the server is
    // listening on 127.0.0.1 causes frequent failures on OS X 10.10.
    this.transport.connectionFlags |= Ci.nsISocketTransport.DISABLE_IPV6;
    this.transport.setEventSink(this, this.thread);
    if (aOriginAttributes) {
      this.transport.originAttributes = aOriginAttributes;
    }
    this.inputStream = null;
    this.outputStream = null;
    this.connected = false;
  }

  Connection.prototype = {
    // nsITransportEventSink
    onTransportStatus(aTransport, aStatus, aProgress, aProgressMax) {
      if (!this.connected && aStatus == Ci.nsISocketTransport.STATUS_CONNECTED_TO) {
        this.connected = true;
        this.outputStream.asyncWait(this, 0, 0, this.thread);
      }
    },

    // nsIInputStreamCallback
    onInputStreamReady(aStream) {
      try {
        // this will throw if the stream has been closed by an error
        let str = NetUtil.readInputStreamToString(aStream, aStream.available());
        Assert.equal(str, "0",
                     "Should have received ASCII '0' from server");
        this.inputStream.close();
        this.outputStream.close();
        this.result = Cr.NS_OK;
      } catch (e) {
        this.result = e.result;
      }
      this.defer.resolve(this);
    },

    // nsIOutputStreamCallback
    onOutputStreamReady(aStream) {
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

    go() {
      this.outputStream = this.transport.openOutputStream(0, 0, 0)
                            .QueryInterface(Ci.nsIAsyncOutputStream);
      return this.defer.promise;
    }
  };

  /* Returns a promise to connect to host that resolves to the result of that
   * connection */
  function connectTo(host) {
    Services.prefs.setCharPref("network.dns.localDomains", host);
    let connection = new Connection(host);
    return connection.go();
  }

  add_test(function() {
    if (aBeforeConnect) {
      aBeforeConnect();
    }
    connectTo(aHost).then(function(conn) {
      info("handling " + aHost);
      let expectedNSResult = aExpectedResult == PRErrorCodeSuccess
                           ? Cr.NS_OK
                           : getXPCOMStatusFromNSS(aExpectedResult);
      Assert.equal(conn.result, expectedNSResult,
                   "Actual and expected connection result should match");
      if (aWithSecurityInfo) {
        aWithSecurityInfo(conn.transport.securityInfo
                              .QueryInterface(Ci.nsITransportSecurityInfo));
      }
      run_next_test();
    });
  });
}

function _getBinaryUtil(binaryUtilName) {
  let utilBin = Services.dirsvc.get("CurProcD", Ci.nsIFile);
  utilBin.append(binaryUtilName + mozinfo.bin_suffix);
  // If we're testing locally, the above works. If not, the server executable
  // is in another location.
  if (!utilBin.exists()) {
    utilBin = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
    while (utilBin.path.indexOf("xpcshell") != -1) {
      utilBin = utilBin.parent;
    }
    utilBin.append("bin");
    utilBin.append(binaryUtilName + mozinfo.bin_suffix);
  }
  // But maybe we're on Android or B2G, where binaries are in /data/local/xpcb.
  if (!utilBin.exists()) {
    utilBin.initWithPath("/data/local/xpcb/");
    utilBin.append(binaryUtilName);
  }
  Assert.ok(utilBin.exists(), `Binary util ${binaryUtilName} should exist`);
  return utilBin;
}

// Do not call this directly; use add_tls_server_setup
function _setupTLSServerTest(serverBinName, certsPath, addDefaultRoot) {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);
  // The trusted CA that is typically used for "good" certificates.
  if (addDefaultRoot) {
    addCertFromFile(certdb, `${certsPath}/test-ca.pem`, "CTu,u,u");
  }

  const CALLBACK_PORT = 8444;

  let envSvc = Cc["@mozilla.org/process/environment;1"]
                 .getService(Ci.nsIEnvironment);
  let greBinDir = Services.dirsvc.get("GreBinD", Ci.nsIFile);
  envSvc.set("DYLD_LIBRARY_PATH", greBinDir.path);
  // TODO(bug 1107794): Android libraries are in /data/local/xpcb, but "GreBinD"
  // does not return this path on Android, so hard code it here.
  envSvc.set("LD_LIBRARY_PATH", greBinDir.path + ":/data/local/xpcb");
  envSvc.set("MOZ_TLS_SERVER_DEBUG_LEVEL", "3");
  envSvc.set("MOZ_TLS_SERVER_CALLBACK_PORT", CALLBACK_PORT);

  let httpServer = new HttpServer();
  httpServer.registerPathHandler("/",
      function handleServerCallback(aRequest, aResponse) {
        aResponse.setStatusLine(aRequest.httpVersion, 200, "OK");
        aResponse.setHeader("Content-Type", "text/plain");
        let responseBody = "OK!";
        aResponse.bodyOutputStream.write(responseBody, responseBody.length);
        executeSoon(function() {
          httpServer.stop(run_next_test);
        });
      });
  httpServer.start(CALLBACK_PORT);

  let serverBin = _getBinaryUtil(serverBinName);
  let process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
  process.init(serverBin);
  let certDir = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
  certDir.append(`${certsPath}`);
  Assert.ok(certDir.exists(), `certificate folder (${certsPath}) should exist`);
  // Using "sql:" causes the SQL DB to be used so we can run tests on Android.
  process.run(false, [ "sql:" + certDir.path ], 1);

  registerCleanupFunction(function() {
    process.kill();
  });
}

// Returns an Array of OCSP responses for a given ocspRespArray and a location
// for a nssDB where the certs and public keys are prepopulated.
// ocspRespArray is an array of arrays like:
// [ [typeOfResponse, certnick, extracertnick, thisUpdateSkew]...]
function generateOCSPResponses(ocspRespArray, nssDBlocation) {
  let utilBinName = "GenerateOCSPResponse";
  let ocspGenBin = _getBinaryUtil(utilBinName);
  let retArray = [];

  for (let i = 0; i < ocspRespArray.length; i++) {
    let argArray = [];
    let ocspFilepre = do_get_file(i.toString() + ".ocsp", true);
    let filename = ocspFilepre.path;
    // Using "sql:" causes the SQL DB to be used so we can run tests on Android.
    argArray.push("sql:" + nssDBlocation);
    argArray.push(ocspRespArray[i][0]); // ocsRespType;
    argArray.push(ocspRespArray[i][1]); // nick;
    argArray.push(ocspRespArray[i][2]); // extranickname
    argArray.push(ocspRespArray[i][3]); // thisUpdate skew
    argArray.push(filename);
    info("argArray = " + argArray);

    let process = Cc["@mozilla.org/process/util;1"]
                    .createInstance(Ci.nsIProcess);
    process.init(ocspGenBin);
    process.run(true, argArray, argArray.length);
    Assert.equal(0, process.exitValue, "Process exit value should be 0");
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
    Assert.ok(false, "HTTP responder should not have been queried");
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
// nssDBLocation is the location of the NSS database from where the OCSP
//   responses will be generated (assumes appropiate keys are present)
// expectedCertNames is an array of nicks of the certs to be responsed
// expectedBasePaths is an optional array that is used to indicate
//   what is the expected base path of the OCSP request.
// expectedMethods is an optional array of methods ("GET" or "POST") indicating
//   by which HTTP method the server is expected to be queried.
// expectedResponseTypes is an optional array of OCSP response types to use (see
//   GenerateOCSPResponse.cpp).
function startOCSPResponder(serverPort, identity, nssDBLocation,
                            expectedCertNames, expectedBasePaths,
                            expectedMethods, expectedResponseTypes) {
  let ocspResponseGenerationArgs = expectedCertNames.map(
    function(expectedNick) {
      let responseType = "good";
      if (expectedResponseTypes && expectedResponseTypes.length >= 1) {
        responseType = expectedResponseTypes.shift();
      }
      return [responseType, expectedNick, "unused", 0];
    }
  );
  let ocspResponses = generateOCSPResponses(ocspResponseGenerationArgs,
                                            nssDBLocation);
  let httpServer = new HttpServer();
  httpServer.registerPrefixHandler("/",
    function handleServerCallback(aRequest, aResponse) {
      info("got request for: " + aRequest.path);
      let basePath = aRequest.path.slice(1).split("/")[0];
      if (expectedBasePaths.length >= 1) {
        Assert.equal(basePath, expectedBasePaths.shift(),
                     "Actual and expected base path should match");
      }
      Assert.ok(expectedCertNames.length >= 1,
                "expectedCertNames should contain >= 1 entries");
      if (expectedMethods && expectedMethods.length >= 1) {
        Assert.equal(aRequest.method, expectedMethods.shift(),
                     "Actual and expected fetch method should match");
      }
      aResponse.setStatusLine(aRequest.httpVersion, 200, "OK");
      aResponse.setHeader("Content-Type", "application/ocsp-response");
      aResponse.write(ocspResponses.shift());
    });
  httpServer.identity.setPrimary("http", identity, serverPort);
  httpServer.start(serverPort);
  return {
    stop(callback) {
      // make sure we consumed each expected response
      Assert.equal(ocspResponses.length, 0,
                   "Should have 0 remaining expected OCSP responses");
      if (expectedMethods) {
        Assert.equal(expectedMethods.length, 0,
                     "Should have 0 remaining expected fetch methods");
      }
      if (expectedBasePaths) {
        Assert.equal(expectedBasePaths.length, 0,
                     "Should have 0 remaining expected base paths");
      }
      if (expectedResponseTypes) {
        Assert.equal(expectedResponseTypes.length, 0,
                     "Should have 0 remaining expected response types");
      }
      httpServer.stop(callback);
    }
  };
}

// A prototype for a fake, error-free sslstatus
var FakeSSLStatus = function(certificate) {
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
  getInterface(aIID) {
    return this.QueryInterface(aIID);
  },
  QueryInterface(aIID) {
    if (aIID.equals(Ci.nsISSLStatus) ||
        aIID.equals(Ci.nsISupports)) {
      return this;
    }
    throw new Error(Cr.NS_ERROR_NO_INTERFACE);
  },
};

// Utility functions for adding tests relating to certificate error overrides

// Helper function for add_cert_override_test. Probably doesn't need to be
// called directly.
function add_cert_override(aHost, aExpectedBits, aExpectedErrorRegexp,
                           aSecurityInfo) {
  if (aExpectedErrorRegexp) {
    info(aSecurityInfo.errorMessage);
    Assert.ok(aExpectedErrorRegexp.test(aSecurityInfo.errorMessage),
              "Actual error message should match expected error regexp");
  }
  let sslstatus = aSecurityInfo.QueryInterface(Ci.nsISSLStatusProvider)
                               .SSLStatus;
  let bits =
    (sslstatus.isUntrusted ? Ci.nsICertOverrideService.ERROR_UNTRUSTED : 0) |
    (sslstatus.isDomainMismatch ? Ci.nsICertOverrideService.ERROR_MISMATCH : 0) |
    (sslstatus.isNotValidAtThisTime ? Ci.nsICertOverrideService.ERROR_TIME : 0);

  Assert.equal(bits, aExpectedBits,
               "Actual and expected override bits should match");
  let cert = sslstatus.serverCert;
  let certOverrideService = Cc["@mozilla.org/security/certoverride;1"]
                              .getService(Ci.nsICertOverrideService);
  certOverrideService.rememberValidityOverride(aHost, 8443, cert, aExpectedBits,
                                               true);
}

// Given a host, expected error bits (see nsICertOverrideService.idl), an
// expected error code, and optionally a regular expression that the resulting
// error message must match, tests that an initial connection to the host fails
// with the expected errors and that adding an override results in a subsequent
// connection succeeding.
function add_cert_override_test(aHost, aExpectedBits, aExpectedError,
                                aExpectedErrorRegexp = undefined,
                                aExpectedSSLStatus = undefined) {
  add_connection_test(aHost, aExpectedError, null,
                      add_cert_override.bind(this, aHost, aExpectedBits,
                                             aExpectedErrorRegexp));
  add_connection_test(aHost, PRErrorCodeSuccess, null, aSecurityInfo => {
    Assert.ok(aSecurityInfo.securityState &
              Ci.nsIWebProgressListener.STATE_CERT_USER_OVERRIDDEN,
              "Cert override flag should be set on the security state");
    if (aExpectedSSLStatus) {
      let sslstatus = aSecurityInfo.QueryInterface(Ci.nsISSLStatusProvider)
                                  .SSLStatus;
      if (aExpectedSSLStatus.failedCertChain) {
        ok(aExpectedSSLStatus.failedCertChain.equals(sslstatus.failedCertChain));
      }
    }
  });
}

// Helper function for add_prevented_cert_override_test. This is much like
// add_cert_override except it may not be the case that the connection has an
// SSLStatus set on it. In this case, the error was not overridable anyway, so
// we consider it a success.
function attempt_adding_cert_override(aHost, aExpectedBits, aSecurityInfo) {
  let sslstatus = aSecurityInfo.QueryInterface(Ci.nsISSLStatusProvider)
                               .SSLStatus;
  if (sslstatus) {
    let bits =
      (sslstatus.isUntrusted ? Ci.nsICertOverrideService.ERROR_UNTRUSTED : 0) |
      (sslstatus.isDomainMismatch ? Ci.nsICertOverrideService.ERROR_MISMATCH : 0) |
      (sslstatus.isNotValidAtThisTime ? Ci.nsICertOverrideService.ERROR_TIME : 0);
    Assert.equal(bits, aExpectedBits,
                 "Actual and expected override bits should match");
    let cert = sslstatus.serverCert;
    let certOverrideService = Cc["@mozilla.org/security/certoverride;1"]
                                .getService(Ci.nsICertOverrideService);
    certOverrideService.rememberValidityOverride(aHost, 8443, cert, aExpectedBits,
                                                 true);
  }
}

// Given a host, expected error bits (see nsICertOverrideService.idl), and
// an expected error code, tests that an initial connection to the host fails
// with the expected errors and that adding an override does not result in a
// subsequent connection succeeding (i.e. the same error code is encountered).
// The idea here is that for HSTS hosts or hosts with key pins, no error is
// overridable, even if an entry is added to the override service.
function add_prevented_cert_override_test(aHost, aExpectedBits, aExpectedError) {
  add_connection_test(aHost, aExpectedError, null,
                      attempt_adding_cert_override.bind(this, aHost, aExpectedBits));
  add_connection_test(aHost, aExpectedError);
}

// Helper for asyncTestCertificateUsages.
class CertVerificationResult {
  constructor(certName, usageString, successExpected, resolve) {
    this.certName = certName;
    this.usageString = usageString;
    this.successExpected = successExpected;
    this.resolve = resolve;
  }

  verifyCertFinished(aPRErrorCode, aVerifiedChain, aHasEVPolicy) {
    if (this.successExpected) {
      equal(aPRErrorCode, PRErrorCodeSuccess,
            `verifying ${this.certName} for ${this.usageString} should succeed`);
    } else {
      notEqual(aPRErrorCode, PRErrorCodeSuccess,
               `verifying ${this.certName} for ${this.usageString} should fail`);
    }
    this.resolve();
  }
}

/**
 * Asynchronously attempts to verify the given certificate for all supported
 * usages (see allCertificateUsages). Verifies that the results match the
 * expected successful usages. Returns a promise that will resolve when all
 * verifications have been performed.
 * Verification happens "now" with no specified flags or hostname.
 *
 * @param {nsIX509CertDB} certdb
 *   The certificate database to use to verify the certificate.
 * @param {nsIX509Cert} cert
 *   The certificate to be verified.
 * @param {Number[]} expectedUsages
 *   A list of usages (as their integer values) that are expected to verify
 *   successfully.
 * @return {Promise}
 *   A promise that will resolve with no value when all asynchronous operations
 *   have completed.
 */
function asyncTestCertificateUsages(certdb, cert, expectedUsages) {
  let now = (new Date()).getTime() / 1000;
  let promises = [];
  Object.keys(allCertificateUsages).forEach(usageString => {
    let promise = new Promise((resolve, reject) => {
      let usage = allCertificateUsages[usageString];
      let successExpected = expectedUsages.includes(usage);
      let result = new CertVerificationResult(cert.commonName, usageString,
                                              successExpected, resolve);
      certdb.asyncVerifyCertAtTime(cert, usage, 0, null, now, result);
    });
    promises.push(promise);
  });
  return Promise.all(promises);
}

/**
 * Loads the pkcs11testmodule.cpp test PKCS #11 module, and registers a cleanup
 * function that unloads it once the calling test completes.
 *
 * @param {Boolean} expectModuleUnloadToFail
 *                  Should be set to true for tests that manually unload the
 *                  test module, so the attempt to auto unload the test module
 *                  doesn't cause a test failure. Should be set to false
 *                  otherwise, so failure to automatically unload the test
 *                  module gets reported.
 */
function loadPKCS11TestModule(expectModuleUnloadToFail) {
  let libraryFile = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
  libraryFile.append("pkcs11testmodule");
  libraryFile.append(ctypes.libraryName("pkcs11testmodule"));
  ok(libraryFile.exists(), "The pkcs11testmodule file should exist");

  let pkcs11ModuleDB = Cc["@mozilla.org/security/pkcs11moduledb;1"]
                         .getService(Ci.nsIPKCS11ModuleDB);
  registerCleanupFunction(() => {
    try {
      pkcs11ModuleDB.deleteModule("PKCS11 Test Module");
    } catch (e) {
      Assert.ok(expectModuleUnloadToFail,
                `Module unload should suceed only when expected: ${e}`);
    }
  });
  pkcs11ModuleDB.addModule("PKCS11 Test Module", libraryFile.path, 0, 0);
}

/**
 * @param {String} data
 * @returns {String}
 */
function hexify(data) {
  // |slice(-2)| chomps off the last two characters of a string.
  // Therefore, if the Unicode value is < 0x10, we have a single-character hex
  // string when we want one that's two characters, and unconditionally
  // prepending a "0" solves the problem.
  return Array.from(data, (c, i) => ("0" + data.charCodeAt(i).toString(16)).slice(-2)).join("");
}

/**
 * @param {String[]} lines
 *        Lines to write. Each line automatically has "\n" appended to it when
 *        being written.
 * @param {nsIFileOutputStream} outputStream
 */
function writeLinesAndClose(lines, outputStream) {
  for (let line of lines) {
    line += "\n";
    outputStream.write(line, line.length);
  }
  outputStream.close();
}
