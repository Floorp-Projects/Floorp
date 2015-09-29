// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// In which we connect to a number of domains (as faked by a server running
// locally) with and without OCSP stapling enabled to determine that good
// things happen and bad things don't.

var gExpectOCSPRequest;

function add_ocsp_test(aHost, aExpectedResult, aStaplingEnabled) {
  add_connection_test(aHost, aExpectedResult,
    function() {
      gExpectOCSPRequest = !aStaplingEnabled;
      clearOCSPCache();
      clearSessionCache();
      Services.prefs.setBoolPref("security.ssl.enable_ocsp_stapling",
                                 aStaplingEnabled);
    });
}

function add_tests() {
  // In the absence of OCSP stapling, these should actually all work.
  add_ocsp_test("ocsp-stapling-good.example.com",
                PRErrorCodeSuccess, false);
  add_ocsp_test("ocsp-stapling-revoked.example.com",
                PRErrorCodeSuccess, false);
  add_ocsp_test("ocsp-stapling-good-other-ca.example.com",
                PRErrorCodeSuccess, false);
  add_ocsp_test("ocsp-stapling-malformed.example.com",
                PRErrorCodeSuccess, false);
  add_ocsp_test("ocsp-stapling-srverr.example.com",
                PRErrorCodeSuccess, false);
  add_ocsp_test("ocsp-stapling-trylater.example.com",
                PRErrorCodeSuccess, false);
  add_ocsp_test("ocsp-stapling-needssig.example.com",
                PRErrorCodeSuccess, false);
  add_ocsp_test("ocsp-stapling-unauthorized.example.com",
                PRErrorCodeSuccess, false);
  add_ocsp_test("ocsp-stapling-unknown.example.com",
                PRErrorCodeSuccess, false);
  add_ocsp_test("ocsp-stapling-good-other.example.com",
                PRErrorCodeSuccess, false);
  add_ocsp_test("ocsp-stapling-none.example.com",
                PRErrorCodeSuccess, false);
  add_ocsp_test("ocsp-stapling-expired.example.com",
                PRErrorCodeSuccess, false);
  add_ocsp_test("ocsp-stapling-expired-fresh-ca.example.com",
                PRErrorCodeSuccess, false);
  add_ocsp_test("ocsp-stapling-skip-responseBytes.example.com",
                PRErrorCodeSuccess, false);
  add_ocsp_test("ocsp-stapling-critical-extension.example.com",
                PRErrorCodeSuccess, false);
  add_ocsp_test("ocsp-stapling-noncritical-extension.example.com",
                PRErrorCodeSuccess, false);
  add_ocsp_test("ocsp-stapling-empty-extensions.example.com",
                PRErrorCodeSuccess, false);

  // Now test OCSP stapling
  // The following error codes are defined in security/nss/lib/util/SECerrs.h

  add_ocsp_test("ocsp-stapling-good.example.com", PRErrorCodeSuccess, true);

  add_ocsp_test("ocsp-stapling-revoked.example.com",
                SEC_ERROR_REVOKED_CERTIFICATE, true);

  // SEC_ERROR_OCSP_INVALID_SIGNING_CERT vs SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE
  // depends on whether the CA that signed the response is a trusted CA
  // (but only with the classic implementation - mozilla::pkix always
  // results in the error SEC_ERROR_OCSP_INVALID_SIGNING_CERT).

  // This stapled response is from a CA that is untrusted and did not issue
  // the server's certificate.
  let certDB = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);
  let otherTestCA = constructCertFromFile("ocsp_certs/other-test-ca.pem");
  add_test(function() {
    certDB.setCertTrust(otherTestCA, Ci.nsIX509Cert.CA_CERT,
                        Ci.nsIX509CertDB.UNTRUSTED);
    run_next_test();
  });
  add_ocsp_test("ocsp-stapling-good-other-ca.example.com",
                SEC_ERROR_OCSP_INVALID_SIGNING_CERT, true);

  // The stapled response is from a CA that is trusted but did not issue the
  // server's certificate.
  add_test(function() {
    certDB.setCertTrust(otherTestCA, Ci.nsIX509Cert.CA_CERT,
                        Ci.nsIX509CertDB.TRUSTED_SSL);
    run_next_test();
  });
  // TODO(bug 979055): When using ByName instead of ByKey, the error here is
  // SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE. We should be testing both cases.
  add_ocsp_test("ocsp-stapling-good-other-ca.example.com",
                SEC_ERROR_OCSP_INVALID_SIGNING_CERT, true);

  // TODO: Test the case where the signing cert can't be found at all, which
  // will result in SEC_ERROR_BAD_DATABASE in the NSS classic case.

  add_ocsp_test("ocsp-stapling-malformed.example.com",
                SEC_ERROR_OCSP_MALFORMED_REQUEST, true);
  add_ocsp_test("ocsp-stapling-srverr.example.com",
                SEC_ERROR_OCSP_SERVER_ERROR, true);
  add_ocsp_test("ocsp-stapling-trylater.example.com",
                SEC_ERROR_OCSP_TRY_SERVER_LATER, true);
  add_ocsp_test("ocsp-stapling-needssig.example.com",
                SEC_ERROR_OCSP_REQUEST_NEEDS_SIG, true);
  add_ocsp_test("ocsp-stapling-unauthorized.example.com",
                SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST, true);
  add_ocsp_test("ocsp-stapling-unknown.example.com",
                SEC_ERROR_OCSP_UNKNOWN_CERT, true);
  add_ocsp_test("ocsp-stapling-good-other.example.com",
                MOZILLA_PKIX_ERROR_OCSP_RESPONSE_FOR_CERT_MISSING, true);
  // If the server doesn't staple an OCSP response, we continue as normal
  // (this means that even though stapling is enabled, we expect an OCSP
  // request).
  add_connection_test("ocsp-stapling-none.example.com", PRErrorCodeSuccess,
    function() {
      gExpectOCSPRequest = true;
      clearOCSPCache();
      clearSessionCache();
      Services.prefs.setBoolPref("security.ssl.enable_ocsp_stapling", true);
    }
  );
  add_ocsp_test("ocsp-stapling-empty.example.com",
                SEC_ERROR_OCSP_MALFORMED_RESPONSE, true);

  add_ocsp_test("ocsp-stapling-skip-responseBytes.example.com",
                SEC_ERROR_OCSP_MALFORMED_RESPONSE, true);

  add_ocsp_test("ocsp-stapling-critical-extension.example.com",
                SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION, true);
  add_ocsp_test("ocsp-stapling-noncritical-extension.example.com",
                PRErrorCodeSuccess, true);
  // TODO(bug 997994): Disallow empty Extensions in responses
  add_ocsp_test("ocsp-stapling-empty-extensions.example.com",
                PRErrorCodeSuccess, true);

  add_ocsp_test("ocsp-stapling-delegated-included.example.com",
                PRErrorCodeSuccess, true);
  add_ocsp_test("ocsp-stapling-delegated-included-last.example.com",
                PRErrorCodeSuccess, true);
  add_ocsp_test("ocsp-stapling-delegated-missing.example.com",
                SEC_ERROR_OCSP_INVALID_SIGNING_CERT, true);
  add_ocsp_test("ocsp-stapling-delegated-missing-multiple.example.com",
                SEC_ERROR_OCSP_INVALID_SIGNING_CERT, true);
  add_ocsp_test("ocsp-stapling-delegated-no-extKeyUsage.example.com",
                SEC_ERROR_OCSP_INVALID_SIGNING_CERT, true);
  add_ocsp_test("ocsp-stapling-delegated-from-intermediate.example.com",
                SEC_ERROR_OCSP_INVALID_SIGNING_CERT, true);
  add_ocsp_test("ocsp-stapling-delegated-keyUsage-crlSigning.example.com",
                SEC_ERROR_OCSP_INVALID_SIGNING_CERT, true);
  add_ocsp_test("ocsp-stapling-delegated-wrong-extKeyUsage.example.com",
                SEC_ERROR_OCSP_INVALID_SIGNING_CERT, true);

  // ocsp-stapling-expired.example.com and
  // ocsp-stapling-expired-fresh-ca.example.com are handled in
  // test_ocsp_stapling_expired.js

  // Check that OCSP responder certificates with key sizes below 1024 bits are
  // rejected, even when the main certificate chain keys are at least 1024 bits.
  add_ocsp_test("keysize-ocsp-delegated.example.com",
                SEC_ERROR_OCSP_INVALID_SIGNING_CERT, true);

  add_ocsp_test("revoked-ca-cert-used-as-end-entity.example.com",
                SEC_ERROR_REVOKED_CERTIFICATE, true);
}

function check_ocsp_stapling_telemetry() {
  let histogram = Cc["@mozilla.org/base/telemetry;1"]
                    .getService(Ci.nsITelemetry)
                    .getHistogramById("SSL_OCSP_STAPLING")
                    .snapshot();
  equal(histogram.counts[0], 0,
        "Should have 0 connections for unused histogram bucket 0");
  equal(histogram.counts[1], 5,
        "Actual and expected connections with a good response should match");
  equal(histogram.counts[2], 18,
        "Actual and expected connections with no stapled response should match");
  equal(histogram.counts[3], 0,
        "Actual and expected connections with an expired response should match");
  equal(histogram.counts[4], 21,
        "Actual and expected connections with bad responses should match");
  run_next_test();
}

function run_test() {
  do_get_profile();


  let fakeOCSPResponder = new HttpServer();
  fakeOCSPResponder.registerPrefixHandler("/", function (request, response) {
    response.setStatusLine(request.httpVersion, 500, "Internal Server Error");
    ok(gExpectOCSPRequest,
       "Should be getting an OCSP request only when expected");
  });
  fakeOCSPResponder.start(8888);

  add_tls_server_setup("OCSPStaplingServer", "ocsp_certs");

  add_tests();

  add_test(function () {
    fakeOCSPResponder.stop(check_ocsp_stapling_telemetry);
  });

  run_next_test();
}
