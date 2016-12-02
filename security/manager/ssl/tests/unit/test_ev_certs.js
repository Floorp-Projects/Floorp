// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// Tests that end-entity certificates that should successfully verify as EV
// (Extended Validation) do so and that end-entity certificates that should not
// successfully verify as EV do not. Also tests related situations (e.g. that
// failure to fetch an OCSP response results in no EV treatment).
//
// A quick note about the certificates in these tests: generally, an EV
// certificate chain will have an end-entity with a specific policy OID followed
// by an intermediate with the anyPolicy OID chaining to a root with no policy
// OID (since it's a trust anchor, it can be omitted). In these tests, the
// specific policy OID is 1.3.6.1.4.1.13769.666.666.666.1.500.9.1 and is
// referred to as the test OID. In order to reflect what will commonly be
// encountered, the end-entity of any given test path will have the test OID
// unless otherwise specified in the name of the test path. Similarly, the
// intermediate will have the anyPolicy OID, again unless otherwise specified.
// For example, for the path where the end-entity does not have an OCSP URI
// (referred to as "no-ocsp-ee-path-{ee,int}", the end-entity has the test OID
// whereas the intermediate has the anyPolicy OID.
// For another example, for the test OID path ("test-oid-path-{ee,int}"), both
// the end-entity and the intermediate have the test OID.

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

do_register_cleanup(() => {
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.clearUserPref("security.OCSP.enabled");
});

Services.prefs.setCharPref("network.dns.localDomains", "www.example.com");
Services.prefs.setIntPref("security.OCSP.enabled", 1);
const evroot = addCertFromFile(certdb, "test_ev_certs/evroot.pem", "CTu,,");
addCertFromFile(certdb, "test_ev_certs/non-evroot-ca.pem", "CTu,,");

const SERVER_PORT = 8888;

function failingOCSPResponder() {
  return getFailingHttpServer(SERVER_PORT, ["www.example.com"]);
}

class EVCertVerificationResult {
  constructor(testcase, expectedPRErrorCode, expectedEV, resolve,
              ocspResponder) {
    this.testcase = testcase;
    this.expectedPRErrorCode = expectedPRErrorCode;
    this.expectedEV = expectedEV;
    this.resolve = resolve;
    this.ocspResponder = ocspResponder;
  }

  verifyCertFinished(prErrorCode, verifiedChain, hasEVPolicy) {
    equal(prErrorCode, this.expectedPRErrorCode,
          `${this.testcase} should have expected error code`);
    equal(hasEVPolicy, this.expectedEV,
          `${this.testcase} should result in expected EV status`);
    this.ocspResponder.stop(this.resolve);
  }
}

function asyncTestEV(cert, expectedPRErrorCode, expectedEV,
                     expectedOCSPRequestPaths, ocspResponseTypes = undefined)
{
  let now = Date.now() / 1000;
  return new Promise((resolve, reject) => {
    let ocspResponder = expectedOCSPRequestPaths.length > 0
                      ? startOCSPResponder(SERVER_PORT, "www.example.com",
                                           "test_ev_certs",
                                           expectedOCSPRequestPaths,
                                           expectedOCSPRequestPaths.slice(),
                                           null, ocspResponseTypes)
                      : failingOCSPResponder();
    let result = new EVCertVerificationResult(cert.subjectName,
                                              expectedPRErrorCode, expectedEV,
                                              resolve, ocspResponder);
    certdb.asyncVerifyCertAtTime(cert, certificateUsageSSLServer, 0,
                                 "ev-test.example.com", now, result);
  });
}

function ensureVerifiesAsEV(testcase) {
  let cert = constructCertFromFile(`test_ev_certs/${testcase}-ee.pem`);
  addCertFromFile(certdb, `test_ev_certs/${testcase}-int.pem`, ",,");
  let expectedOCSPRequestPaths = gEVExpected
                               ? [ `${testcase}-int`, `${testcase}-ee` ]
                               : [ `${testcase}-ee` ];
  return asyncTestEV(cert, PRErrorCodeSuccess, gEVExpected,
                     expectedOCSPRequestPaths);
}

function ensureVerifiesAsEVWithNoOCSPRequests(testcase) {
  let cert = constructCertFromFile(`test_ev_certs/${testcase}-ee.pem`);
  addCertFromFile(certdb, `test_ev_certs/${testcase}-int.pem`, ",,");
  return asyncTestEV(cert, PRErrorCodeSuccess, gEVExpected, []);
}

function ensureVerifiesAsDV(testcase, expectedOCSPRequestPaths = undefined) {
  let cert = constructCertFromFile(`test_ev_certs/${testcase}-ee.pem`);
  addCertFromFile(certdb, `test_ev_certs/${testcase}-int.pem`, ",,");
  return asyncTestEV(cert, PRErrorCodeSuccess, false,
                     expectedOCSPRequestPaths ? expectedOCSPRequestPaths
                                              : [ `${testcase}-ee` ]);
}

function ensureVerificationFails(testcase, expectedPRErrorCode) {
  let cert = constructCertFromFile(`test_ev_certs/${testcase}-ee.pem`);
  addCertFromFile(certdb, `test_ev_certs/${testcase}-int.pem`, ",,");
  return asyncTestEV(cert, expectedPRErrorCode, false, []);
}

function verifyWithFlags_LOCAL_ONLY_and_MUST_BE_EV(testcase, expectSuccess) {
  let cert = constructCertFromFile(`test_ev_certs/${testcase}-ee.pem`);
  addCertFromFile(certdb, `test_ev_certs/${testcase}-int.pem`, ",,");
  let now = Date.now() / 1000;
  let expectedErrorCode = SEC_ERROR_POLICY_VALIDATION_FAILED;
  if (expectSuccess && gEVExpected) {
    expectedErrorCode = PRErrorCodeSuccess;
  }
  return new Promise((resolve, reject) => {
    let ocspResponder = failingOCSPResponder();
    let result = new EVCertVerificationResult(
      cert.subjectName, expectedErrorCode, expectSuccess && gEVExpected,
      resolve, ocspResponder);
    let flags = Ci.nsIX509CertDB.FLAG_LOCAL_ONLY |
                Ci.nsIX509CertDB.FLAG_MUST_BE_EV;
    certdb.asyncVerifyCertAtTime(cert, certificateUsageSSLServer, flags,
                                 "ev-test.example.com", now, result);
  });
}

function ensureNoOCSPMeansNoEV(testcase) {
  return verifyWithFlags_LOCAL_ONLY_and_MUST_BE_EV(testcase, false);
}

function ensureVerifiesAsEVWithFLAG_LOCAL_ONLY(testcase) {
  return verifyWithFlags_LOCAL_ONLY_and_MUST_BE_EV(testcase, true);
}

function ensureOneCRLSkipsOCSPForIntermediates(testcase) {
  let cert = constructCertFromFile(`test_ev_certs/${testcase}-ee.pem`);
  addCertFromFile(certdb, `test_ev_certs/${testcase}-int.pem`, ",,");
  return asyncTestEV(cert, PRErrorCodeSuccess, gEVExpected,
                     [ `${testcase}-ee` ]);
}

function verifyWithDifferentOCSPResponseTypes(testcase, responses, expectEV) {
  let cert = constructCertFromFile(`test_ev_certs/${testcase}-ee.pem`);
  addCertFromFile(certdb, `test_ev_certs/${testcase}-int.pem`, ",,");
  let expectedOCSPRequestPaths = gEVExpected
                               ? [ `${testcase}-int`, `${testcase}-ee` ]
                               : [ `${testcase}-ee` ];
  let ocspResponseTypes = gEVExpected ? responses : responses.slice(1);
  return asyncTestEV(cert, PRErrorCodeSuccess, gEVExpected && expectEV,
                     expectedOCSPRequestPaths, ocspResponseTypes);
}

function ensureVerifiesAsEVWithOldIntermediateOCSPResponse(testcase) {
  return verifyWithDifferentOCSPResponseTypes(
    testcase, [ "longvalidityalmostold", "good" ], true);
}

function ensureVerifiesAsDVWithOldEndEntityOCSPResponse(testcase) {
  return verifyWithDifferentOCSPResponseTypes(
    testcase, [ "good", "longvalidityalmostold" ], false);
}

function ensureVerifiesAsDVWithVeryOldEndEntityOCSPResponse(testcase) {
  return verifyWithDifferentOCSPResponseTypes(
    testcase, [ "good", "ancientstillvalid" ], false);
}

// These should all verify as EV.
add_task(function* plainExpectSuccessEVTests() {
  yield ensureVerifiesAsEV("anyPolicy-int-path");
  yield ensureVerifiesAsEV("test-oid-path");
  yield ensureVerifiesAsEV("cabforum-oid-path");
  yield ensureVerifiesAsEV("cabforum-and-test-oid-ee-path");
  yield ensureVerifiesAsEV("test-and-cabforum-oid-ee-path");
  yield ensureVerifiesAsEV("reverse-order-oids-path");
  // In this case, the end-entity has both the CA/B Forum OID and the test OID
  // (in that order). The intermediate has the CA/B Forum OID. Since the
  // implementation uses the first EV policy it encounters in the end-entity as
  // the required one, this successfully verifies as EV.
  yield ensureVerifiesAsEV("cabforum-and-test-oid-ee-cabforum-oid-int-path");
});

// These fail for various reasons to verify as EV, but fallback to DV should
// succeed.
add_task(function* expectDVFallbackTests() {
  yield ensureVerifiesAsDV("anyPolicy-ee-path");
  yield ensureVerifiesAsDV("non-ev-root-path");
  yield ensureVerifiesAsDV("no-ocsp-ee-path",
                           gEVExpected ? [ "no-ocsp-ee-path-int" ] : []);
  yield ensureVerifiesAsDV("no-ocsp-int-path");
  // In this case, the end-entity has the test OID and the intermediate has the
  // CA/B Forum OID. Since the CA/B Forum OID is not treated the same as the
  // anyPolicy OID, this will not verify as EV.
  yield ensureVerifiesAsDV("test-oid-ee-cabforum-oid-int-path");
  // In this case, the end-entity has both the test OID and the CA/B Forum OID
  // (in that order). The intermediate has only the CA/B Forum OID. Since the
  // implementation uses the first EV policy it encounters in the end-entity as
  // the required one, this fails to verify as EV.
  yield ensureVerifiesAsDV("test-and-cabforum-oid-ee-cabforum-oid-int-path");
});

// Test that removing the trust bits from an EV root causes verifications
// relying on that root to fail (and then test that adding back the trust bits
// causes the verifications to succeed again).
add_task(function* evRootTrustTests() {
  clearOCSPCache();
  do_print("untrusting evroot");
  certdb.setCertTrust(evroot, Ci.nsIX509Cert.CA_CERT,
                      Ci.nsIX509CertDB.UNTRUSTED);
  yield ensureVerificationFails("test-oid-path", SEC_ERROR_UNKNOWN_ISSUER);
  do_print("re-trusting evroot");
  certdb.setCertTrust(evroot, Ci.nsIX509Cert.CA_CERT,
                      Ci.nsIX509CertDB.TRUSTED_SSL);
  yield ensureVerifiesAsEV("test-oid-path");
});

// Test that if FLAG_LOCAL_ONLY and FLAG_MUST_BE_EV are specified, that no OCSP
// requests are made (this also means that nothing will verify as EV).
add_task(function* localOnlyMustBeEVTests() {
  clearOCSPCache();
  yield ensureNoOCSPMeansNoEV("anyPolicy-ee-path");
  yield ensureNoOCSPMeansNoEV("anyPolicy-int-path");
  yield ensureNoOCSPMeansNoEV("non-ev-root-path");
  yield ensureNoOCSPMeansNoEV("no-ocsp-ee-path");
  yield ensureNoOCSPMeansNoEV("no-ocsp-int-path");
  yield ensureNoOCSPMeansNoEV("test-oid-path");
});


// Under certain conditions, OneCRL allows us to skip OCSP requests for
// intermediates.
add_task(function* oneCRLTests() {
  clearOCSPCache();

  // enable OneCRL OCSP skipping - allow staleness of up to 30 hours
  Services.prefs.setIntPref("security.onecrl.maximum_staleness_in_seconds",
                            108000);
  // set the blocklist-background-update-timer value to the recent past
  Services.prefs.setIntPref("services.blocklist.onecrl.checked",
                            Math.floor(Date.now() / 1000) - 1);
  Services.prefs.setIntPref(
    "app.update.lastUpdateTime.blocklist-background-update-timer",
    Math.floor(Date.now() / 1000) - 1);

  yield ensureOneCRLSkipsOCSPForIntermediates("anyPolicy-int-path");
  yield ensureOneCRLSkipsOCSPForIntermediates("no-ocsp-int-path");
  yield ensureOneCRLSkipsOCSPForIntermediates("test-oid-path");

  clearOCSPCache();
  // disable OneCRL OCSP Skipping (no staleness allowed)
  Services.prefs.setIntPref("security.onecrl.maximum_staleness_in_seconds", 0);
  yield ensureVerifiesAsEV("anyPolicy-int-path");
  // Because the intermediate in this case is missing an OCSP URI, it will not
  // validate as EV, but it should fall back to DV.
  yield ensureVerifiesAsDV("no-ocsp-int-path");
  yield ensureVerifiesAsEV("test-oid-path");

  clearOCSPCache();
  // enable OneCRL OCSP skipping - allow staleness of up to 30 hours
  Services.prefs.setIntPref("security.onecrl.maximum_staleness_in_seconds",
                            108000);
  // set the blocklist-background-update-timer value to the more distant past
  Services.prefs.setIntPref("services.blocklist.onecrl.checked",
                            Math.floor(Date.now() / 1000) - 108080);
  Services.prefs.setIntPref(
    "app.update.lastUpdateTime.blocklist-background-update-timer",
    Math.floor(Date.now() / 1000) - 108080);
  yield ensureVerifiesAsEV("anyPolicy-int-path");
  yield ensureVerifiesAsDV("no-ocsp-int-path");
  yield ensureVerifiesAsEV("test-oid-path");

  clearOCSPCache();
  // test that setting "security.onecrl.via.amo" results in the correct
  // OCSP behavior when services.blocklist.onecrl.checked is in the distant past
  // and blacklist-background-update-timer is recent
  Services.prefs.setBoolPref("security.onecrl.via.amo", false);
  // enable OneCRL OCSP skipping - allow staleness of up to 30 hours
  Services.prefs.setIntPref("security.onecrl.maximum_staleness_in_seconds",
                            108000);
  // set the blocklist-background-update-timer value to the recent past
  // (services.blocklist.onecrl.checked defaults to 0)
  Services.prefs.setIntPref(
    "app.update.lastUpdateTime.blocklist-background-update-timer",
    Math.floor(Date.now() / 1000) - 1);

  yield ensureVerifiesAsEV("anyPolicy-int-path");
  yield ensureVerifiesAsDV("no-ocsp-int-path");
  yield ensureVerifiesAsEV("test-oid-path");

  clearOCSPCache();
  // test that setting "security.onecrl.via.amo" results in the correct
  // OCSP behavior when services.blocklist.onecrl.checked is recent
  Services.prefs.setBoolPref("security.onecrl.via.amo", false);
  // enable OneCRL OCSP skipping - allow staleness of up to 30 hours
  Services.prefs.setIntPref("security.onecrl.maximum_staleness_in_seconds",
                            108000);
  // now set services.blocklist.onecrl.checked to a recent value
  Services.prefs.setIntPref("services.blocklist.onecrl.checked",
                            Math.floor(Date.now() / 1000) - 1);
  yield ensureOneCRLSkipsOCSPForIntermediates("anyPolicy-int-path");
  yield ensureOneCRLSkipsOCSPForIntermediates("no-ocsp-int-path");
  yield ensureOneCRLSkipsOCSPForIntermediates("test-oid-path");

  Services.prefs.clearUserPref("security.onecrl.via.amo");
  Services.prefs.clearUserPref("security.onecrl.maximum_staleness_in_seconds");
  Services.prefs.clearUserPref("services.blocklist.onecrl.checked");
  Services.prefs.clearUserPref(
    "app.update.lastUpdateTime.blocklist-background-update-timer");
});

// Prime the OCSP cache and then ensure that we can validate certificates as EV
// without hitting the network. There's two cases here: one where we simply
// validate like normal and then check that the network was never accessed and
// another where we use flags to mandate that the network not be used.
add_task(function* ocspCachingTests() {
  clearOCSPCache();

  yield ensureVerifiesAsEV("anyPolicy-int-path");
  yield ensureVerifiesAsEV("test-oid-path");

  yield ensureVerifiesAsEVWithNoOCSPRequests("anyPolicy-int-path");
  yield ensureVerifiesAsEVWithNoOCSPRequests("test-oid-path");

  yield ensureVerifiesAsEVWithFLAG_LOCAL_ONLY("anyPolicy-int-path");
  yield ensureVerifiesAsEVWithFLAG_LOCAL_ONLY("test-oid-path");
});

// Old-but-still-valid OCSP responses are accepted for intermediates but not
// end-entity certificates (because of OCSP soft-fail this results in DV
// fallback).
add_task(function* oldOCSPResponseTests() {
  clearOCSPCache();

  yield ensureVerifiesAsEVWithOldIntermediateOCSPResponse("anyPolicy-int-path");
  yield ensureVerifiesAsEVWithOldIntermediateOCSPResponse("test-oid-path");

  clearOCSPCache();
  yield ensureVerifiesAsDVWithOldEndEntityOCSPResponse("anyPolicy-int-path");
  yield ensureVerifiesAsDVWithOldEndEntityOCSPResponse("test-oid-path");

  clearOCSPCache();
  yield ensureVerifiesAsDVWithVeryOldEndEntityOCSPResponse(
    "anyPolicy-int-path");
  yield ensureVerifiesAsDVWithVeryOldEndEntityOCSPResponse("test-oid-path");
});
