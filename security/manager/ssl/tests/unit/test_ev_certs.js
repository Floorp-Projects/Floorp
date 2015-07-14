// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

const evrootnick = "XPCShell EV Testing (untrustworthy) CA - Mozilla - " +
                   "EV debug test CA";

// This is the list of certificates needed for the test
// The certificates prefixed by 'int-' are intermediates
let certList = [
  // Test for successful EV validation
  'int-ev-valid',
  'ev-valid',
  'ev-valid-anypolicy-int',
  'int-ev-valid-anypolicy-int',
  'no-ocsp-url-cert', // a cert signed by the EV auth that has no OCSP url
                      // but that contains a valid CRLDP.

  // Testing a root that looks like EV but is not EV enabled
  'int-non-ev-root',
  'non-ev-root',
];

function load_ca(ca_name) {
  var ca_filename = ca_name + ".der";
  addCertFromFile(certdb, "test_ev_certs/" + ca_filename, 'CTu,CTu,CTu');
}

const SERVER_PORT = 8888;

function failingOCSPResponder() {
  return getFailingHttpServer(SERVER_PORT,
                              ["www.example.com", "crl.example.com"]);
}

function start_ocsp_responder(expectedCertNames) {
  let expectedPaths = expectedCertNames.slice();
  return startOCSPResponder(SERVER_PORT, "www.example.com", ["crl.example.com"],
                            "test_ev_certs", expectedCertNames, expectedPaths);
}

function check_cert_err(cert_name, expected_error) {
  let cert = certdb.findCertByNickname(null, cert_name);
  checkCertErrorGeneric(certdb, cert, expected_error, certificateUsageSSLServer);
}


function check_ee_for_ev(cert_name, expected_ev) {
  let cert = certdb.findCertByNickname(null, cert_name);
  checkEVStatus(certdb, cert, certificateUsageSSLServer, expected_ev);
}

function run_test() {
  for (let i = 0 ; i < certList.length; i++) {
    let cert_filename = certList[i] + ".der";
    addCertFromFile(certdb, "test_ev_certs/" + cert_filename, ',,');
  }
  load_ca("evroot");
  load_ca("non-evroot-ca");

  // setup and start ocsp responder
  Services.prefs.setCharPref("network.dns.localDomains",
                             'www.example.com, crl.example.com');
  Services.prefs.setIntPref("security.OCSP.enabled", 1);

  add_test(function () {
    clearOCSPCache();
    let ocspResponder = start_ocsp_responder(
                          gEVExpected ? ["int-ev-valid", "ev-valid"]
                                      : ["ev-valid"]);
    check_ee_for_ev("ev-valid", gEVExpected);
    ocspResponder.stop(run_next_test);
  });


  add_test(function () {
    clearOCSPCache();

    let ocspResponder = start_ocsp_responder(
                          gEVExpected ? ["int-ev-valid-anypolicy-int", "ev-valid-anypolicy-int"]
                                      : ["ev-valid-anypolicy-int"]);
    check_ee_for_ev("ev-valid-anypolicy-int", gEVExpected);
    ocspResponder.stop(run_next_test);
  });

  add_test(function() {
    clearOCSPCache();
    let ocspResponder = start_ocsp_responder(["non-ev-root"]);
    check_ee_for_ev("non-ev-root", false);
    ocspResponder.stop(run_next_test);
  });

  add_test(function() {
    clearOCSPCache();
    let ocspResponder = gEVExpected ? start_ocsp_responder(["int-ev-valid"])
                                    : failingOCSPResponder();
    check_ee_for_ev("no-ocsp-url-cert", false);
    ocspResponder.stop(run_next_test);
  });

  // bug 917380: Check that explicitly removing trust from an EV root actually
  // causes the root to be untrusted.
  const nsIX509Cert = Ci.nsIX509Cert;
  add_test(function() {
    let evRootCA = certdb.findCertByNickname(null, evrootnick);
    certdb.setCertTrust(evRootCA, nsIX509Cert.CA_CERT, 0);

    clearOCSPCache();
    let ocspResponder = failingOCSPResponder();
    check_cert_err("ev-valid", SEC_ERROR_UNKNOWN_ISSUER);
    ocspResponder.stop(run_next_test);
  });

  // bug 917380: Check that a trusted EV root is trusted after disabling and
  // re-enabling trust.
  add_test(function() {
    let evRootCA = certdb.findCertByNickname(null, evrootnick);
    certdb.setCertTrust(evRootCA, nsIX509Cert.CA_CERT,
                        Ci.nsIX509CertDB.TRUSTED_SSL |
                        Ci.nsIX509CertDB.TRUSTED_EMAIL |
                        Ci.nsIX509CertDB.TRUSTED_OBJSIGN);

    clearOCSPCache();
    let ocspResponder = start_ocsp_responder(
                          gEVExpected ? ["int-ev-valid", "ev-valid"]
                                      : ["ev-valid"]);
    check_ee_for_ev("ev-valid", gEVExpected);
    ocspResponder.stop(run_next_test);
  });

  add_test(function () {
    check_no_ocsp_requests("ev-valid", SEC_ERROR_POLICY_VALIDATION_FAILED);
  });

  add_test(function () {
    check_no_ocsp_requests("non-ev-root", SEC_ERROR_POLICY_VALIDATION_FAILED);
  });

  add_test(function () {
    check_no_ocsp_requests("no-ocsp-url-cert", SEC_ERROR_POLICY_VALIDATION_FAILED);
  });

  // Check OneCRL OCSP request skipping works correctly
  add_test(function () {
    // enable OneCRL OCSP skipping - allow staleness of up to 1 day
    Services.prefs.setIntPref("security.onecrl.maximum_staleness_in_seconds", 86400);
    // set the blocklist-background-update-timer value to the recent past
    Services.prefs.setIntPref("app.update.lastUpdateTime.blocklist-background-update-timer",
                              Math.floor(Date.now() / 1000) - 1);
    clearOCSPCache();
    // the intermediate should not have an associated OCSP request
    let ocspResponder = start_ocsp_responder(["ev-valid"]);
    check_ee_for_ev("ev-valid", gEVExpected);
    Services.prefs.clearUserPref("security.onecrl.maximum_staleness_in_seconds");
    ocspResponder.stop(run_next_test);
  });

  add_test(function () {
    // disable OneCRL OCSP Skipping (no staleness allowed)
    Services.prefs.setIntPref("security.onecrl.maximum_staleness_in_seconds", 0);
    clearOCSPCache();
    let ocspResponder = start_ocsp_responder(
                          gEVExpected ? ["int-ev-valid", "ev-valid"]
                                      : ["ev-valid"]);
    check_ee_for_ev("ev-valid", gEVExpected);
    Services.prefs.clearUserPref("security.onecrl.maximum_staleness_in_seconds");
    ocspResponder.stop(run_next_test);
  });

  add_test(function () {
    // enable OneCRL OCSP skipping - allow staleness of up to 1 day
    Services.prefs.setIntPref("security.onecrl.maximum_staleness_in_seconds", 86400);
    // set the blocklist-background-update-timer value to the more distant past
    Services.prefs.setIntPref("app.update.lastUpdateTime.blocklist-background-update-timer",
                              Math.floor(Date.now() / 1000) - 86480);
    clearOCSPCache();
    let ocspResponder = start_ocsp_responder(
                          gEVExpected ? ["int-ev-valid", "ev-valid"]
                                      : ["ev-valid"]);
    check_ee_for_ev("ev-valid", gEVExpected);
    Services.prefs.clearUserPref("security.onecrl.maximum_staleness_in_seconds");
    ocspResponder.stop(run_next_test);
  });

  // Test the EV continues to work with flags after successful EV verification
  add_test(function () {
    clearOCSPCache();
    let ocspResponder = start_ocsp_responder(
                          gEVExpected ? ["int-ev-valid", "ev-valid"]
                                      : ["ev-valid"]);
    check_ee_for_ev("ev-valid", gEVExpected);
    ocspResponder.stop(function () {
      // without net it must be able to EV verify
      let failingOcspResponder = failingOCSPResponder();
      let cert = certdb.findCertByNickname(null, "ev-valid");
      let hasEVPolicy = {};
      let verifiedChain = {};
      let flags = Ci.nsIX509CertDB.FLAG_LOCAL_ONLY |
                  Ci.nsIX509CertDB.FLAG_MUST_BE_EV;

      let error = certdb.verifyCertNow(cert, certificateUsageSSLServer, flags,
                                       null, verifiedChain, hasEVPolicy);
      equal(hasEVPolicy.value, gEVExpected,
            "Actual and expected EV status should match for local only EV");
      equal(error,
            gEVExpected ? PRErrorCodeSuccess : SEC_ERROR_POLICY_VALIDATION_FAILED,
            "Actual and expected error code should match for local only EV");
      failingOcspResponder.stop(run_next_test);
    });
  });

  // Bug 991815 old but valid intermediates are OK
  add_test(function () {
    clearOCSPCache();
    let ocspResponder = startOCSPResponder(SERVER_PORT, "www.example.com", [],
                          "test_ev_certs",
                          gEVExpected ? ["int-ev-valid", "ev-valid"]
                                      : ["ev-valid"],
                          [], [],
                          gEVExpected ? ["longvalidityalmostold", "good"]
                                      : ["good"]);
    check_ee_for_ev("ev-valid", gEVExpected);
    ocspResponder.stop(run_next_test);
  });

  // Bug 991815 old but valid end-entities are NOT OK for EV
  // Unfortunately because of soft-fail we consider these OK for DV.
  add_test(function () {
    clearOCSPCache();
    // Since Mozilla::pkix does not consider the old almost invalid OCSP
    // response valid, it does not cache the old response and thus
    // makes a separate request for DV
    let debugCertNickArray = ["int-ev-valid", "ev-valid", "ev-valid"];
    let debugResponseArray = ["good", "longvalidityalmostold",
                              "longvalidityalmostold"];
    let ocspResponder = startOCSPResponder(SERVER_PORT, "www.example.com", [],
                          "test_ev_certs",
                          gEVExpected ? debugCertNickArray : ["ev-valid"],
                          [], [],
                          gEVExpected ? debugResponseArray
                                      : ["longvalidityalmostold"]);
    check_ee_for_ev("ev-valid", false);
    ocspResponder.stop(run_next_test);
  });

  // Bug 991815 Valid but Ancient (almost two year old) responses are Not OK for
  // EV (still OK for soft fail DV)
  add_test(function () {
    clearOCSPCache();
    let debugCertNickArray = ["int-ev-valid", "ev-valid", "ev-valid"];
    let debugResponseArray = ["good", "ancientstillvalid",
                              "ancientstillvalid"];
    let ocspResponder = startOCSPResponder(SERVER_PORT, "www.example.com", [],
                          "test_ev_certs",
                          gEVExpected ? debugCertNickArray : ["ev-valid"],
                          [], [],
                          gEVExpected ? debugResponseArray
                                      : ["ancientstillvalid"]);
    check_ee_for_ev("ev-valid", false);
    ocspResponder.stop(run_next_test);
  });

  run_next_test();
}

// bug 950240: add FLAG_MUST_BE_EV to CertVerifier::VerifyCert
// to prevent spurious OCSP requests that race with OCSP stapling.
// This has the side-effect of saying an EV certificate is not EV if
// it hasn't already been verified (e.g. on the verification thread when
// connecting to a site).
// This flag is mostly a hack that should be removed once FLAG_LOCAL_ONLY
// works as intended.
function check_no_ocsp_requests(cert_name, expected_error) {
  clearOCSPCache();
  let ocspResponder = failingOCSPResponder();
  let cert = certdb.findCertByNickname(null, cert_name);
  let hasEVPolicy = {};
  let verifiedChain = {};
  let flags = Ci.nsIX509CertDB.FLAG_LOCAL_ONLY |
              Ci.nsIX509CertDB.FLAG_MUST_BE_EV;
  let error = certdb.verifyCertNow(cert, certificateUsageSSLServer, flags,
                                   null, verifiedChain, hasEVPolicy);
  // Since we're not doing OCSP requests, no certificate will be EV.
  equal(hasEVPolicy.value, false,
        "EV status should be false when not doing OCSP requests");
  equal(error, expected_error,
        "Actual and expected error should match when not doing OCSP requests");
  ocspResponder.stop(run_next_test);
}
