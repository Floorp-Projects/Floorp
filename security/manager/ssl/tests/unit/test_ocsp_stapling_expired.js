// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// In which we connect to a number of domains (as faked by a server running
// locally) with OCSP stapling enabled to determine that good things happen
// and bad things don't, specifically with respect to various expired OCSP
// responses (stapled and otherwise).
// According to RFC 6066, if a stapled OCSP response can't be satisfactorilly
// verified, the client should terminate the connection. Unfortunately, due to
// some bugs where servers will staple any old garbage without verifying it, we
// can't be this strict in practice. Originally this caveat only applied to
// expired responses, but recent high-profile failures have caused us to expand
// this to "try later" responses and responses where the signing certificate
// doesn't verify successfully.


var gCurrentOCSPResponse = null;
var gOCSPRequestCount = 0;

function add_ocsp_test(aHost, aExpectedResult, aOCSPResponseToServe,
                       aExpectedRequestCount) {
  add_connection_test(aHost, aExpectedResult,
    function() {
      clearOCSPCache();
      clearSessionCache();
      gCurrentOCSPResponse = aOCSPResponseToServe;
      gOCSPRequestCount = 0;
    },
    function() {
      equal(gOCSPRequestCount, aExpectedRequestCount,
            "Should have made " + aExpectedRequestCount +
            " fallback OCSP request" + (aExpectedRequestCount == 1 ? "" : "s"));
    });
}

do_get_profile();
Services.prefs.setBoolPref("security.ssl.enable_ocsp_stapling", true);
Services.prefs.setIntPref("security.OCSP.enabled", 1);
// Sometimes this test will fail on android due to an OCSP request timing out.
// That aspect of OCSP requests is not what we're testing here, so we can just
// bump the timeout and hopefully avoid these failures.
Services.prefs.setIntPref("security.OCSP.timeoutMilliseconds.soft", 5000);
Services.prefs.setIntPref("security.pki.sha1_enforcement_level", 4);
var args = [["good", "default-ee", "unused", 0],
             ["expiredresponse", "default-ee", "unused", 0],
             ["oldvalidperiod", "default-ee", "unused", 0],
             ["revoked", "default-ee", "unused", 0],
             ["unknown", "default-ee", "unused", 0],
             ["good", "must-staple-ee", "unused", 0],
            ];
var ocspResponses = generateOCSPResponses(args, "ocsp_certs");
// Fresh response, certificate is good.
var ocspResponseGood = ocspResponses[0];
// Expired response, certificate is good.
var expiredOCSPResponseGood = ocspResponses[1];
// Fresh signature, old validity period, certificate is good.
var oldValidityPeriodOCSPResponseGood = ocspResponses[2];
// Fresh signature, certificate is revoked.
var ocspResponseRevoked = ocspResponses[3];
// Fresh signature, certificate is unknown.
var ocspResponseUnknown = ocspResponses[4];
var ocspResponseGoodMustStaple = ocspResponses[5];

// sometimes we expect a result without re-fetch
var willNotRetry = 1;
// but sometimes, since a bad response is in the cache, OCSP fetch will be
// attempted for each validation - in practice, for these test certs, this
// means 6 requests because various hash algorithm and key size combinations
// are tried.
var willRetry = 6;

function run_test() {
  let ocspResponder = new HttpServer();
  ocspResponder.registerPrefixHandler("/", function(request, response) {
    if (gCurrentOCSPResponse) {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "application/ocsp-response");
      response.write(gCurrentOCSPResponse);
    } else {
      response.setStatusLine(request.httpVersion, 500, "Internal Server Error");
      response.write("Internal Server Error");
    }
    gOCSPRequestCount++;
  });
  ocspResponder.start(8888);
  add_tls_server_setup("OCSPStaplingServer", "ocsp_certs");

  // In these tests, the OCSP stapling server gives us a stapled
  // response based on the host name ("ocsp-stapling-expired" or
  // "ocsp-stapling-expired-fresh-ca"). We then ensure that we're
  // properly falling back to fetching revocation information.
  // For ocsp-stapling-expired.example.com, the OCSP stapling server
  // staples an expired OCSP response. The certificate has not expired.
  // For ocsp-stapling-expired-fresh-ca.example.com, the OCSP stapling
  // server staples an OCSP response with a recent signature but with an
  // out-of-date validity period. The certificate has not expired.
  add_ocsp_test("ocsp-stapling-expired.example.com", PRErrorCodeSuccess,
                ocspResponseGood, willNotRetry);
  add_ocsp_test("ocsp-stapling-expired-fresh-ca.example.com", PRErrorCodeSuccess,
                ocspResponseGood, willNotRetry);
  // if we can't fetch a more recent response when
  // given an expired stapled response, we terminate the connection.
  add_ocsp_test("ocsp-stapling-expired.example.com",
                SEC_ERROR_OCSP_OLD_RESPONSE,
                expiredOCSPResponseGood, willRetry);
  add_ocsp_test("ocsp-stapling-expired-fresh-ca.example.com",
                SEC_ERROR_OCSP_OLD_RESPONSE,
                expiredOCSPResponseGood, willRetry);
  add_ocsp_test("ocsp-stapling-expired.example.com",
                SEC_ERROR_OCSP_OLD_RESPONSE,
                oldValidityPeriodOCSPResponseGood, willRetry);
  add_ocsp_test("ocsp-stapling-expired-fresh-ca.example.com",
                SEC_ERROR_OCSP_OLD_RESPONSE,
                oldValidityPeriodOCSPResponseGood, willRetry);
  add_ocsp_test("ocsp-stapling-expired.example.com",
                SEC_ERROR_OCSP_OLD_RESPONSE,
                null, willNotRetry);
  add_ocsp_test("ocsp-stapling-expired.example.com",
                SEC_ERROR_OCSP_OLD_RESPONSE,
                null, willNotRetry);
  // Of course, if the newer response indicates Revoked or Unknown,
  // that status must be returned.
  add_ocsp_test("ocsp-stapling-expired.example.com",
                SEC_ERROR_REVOKED_CERTIFICATE,
                ocspResponseRevoked, willNotRetry);
  add_ocsp_test("ocsp-stapling-expired-fresh-ca.example.com",
                SEC_ERROR_REVOKED_CERTIFICATE,
                ocspResponseRevoked, willNotRetry);
  add_ocsp_test("ocsp-stapling-expired.example.com",
                SEC_ERROR_OCSP_UNKNOWN_CERT,
                ocspResponseUnknown, willRetry);
  add_ocsp_test("ocsp-stapling-expired-fresh-ca.example.com",
                SEC_ERROR_OCSP_UNKNOWN_CERT,
                ocspResponseUnknown, willRetry);

  // If the response is expired but indicates Revoked or Unknown and a
  // newer status can't be fetched, the Revoked or Unknown status will
  // be returned.
  add_ocsp_test("ocsp-stapling-revoked-old.example.com",
                SEC_ERROR_REVOKED_CERTIFICATE,
                null, willNotRetry);
  add_ocsp_test("ocsp-stapling-unknown-old.example.com",
                SEC_ERROR_OCSP_UNKNOWN_CERT,
                null, willNotRetry);
  // If the response is expired but indicates Revoked or Unknown and
  // a newer status can be fetched and successfully verified, this
  // should result in a successful certificate verification.
  add_ocsp_test("ocsp-stapling-revoked-old.example.com", PRErrorCodeSuccess,
                ocspResponseGood, willNotRetry);
  add_ocsp_test("ocsp-stapling-unknown-old.example.com", PRErrorCodeSuccess,
                ocspResponseGood, willNotRetry);
  // If a newer status can be fetched but it fails to verify, the
  // Revoked or Unknown status of the expired stapled response
  // should be returned.
  add_ocsp_test("ocsp-stapling-revoked-old.example.com",
                SEC_ERROR_REVOKED_CERTIFICATE,
                expiredOCSPResponseGood, willRetry);
  add_ocsp_test("ocsp-stapling-unknown-old.example.com",
                SEC_ERROR_OCSP_UNKNOWN_CERT,
                expiredOCSPResponseGood, willRetry);

  // These tests are verifying that an valid but very old response
  // is rejected as a valid stapled response, requiring a fetch
  // from the ocsp responder.
  add_ocsp_test("ocsp-stapling-ancient-valid.example.com", PRErrorCodeSuccess,
                ocspResponseGood, willNotRetry);
  add_ocsp_test("ocsp-stapling-ancient-valid.example.com",
                SEC_ERROR_REVOKED_CERTIFICATE,
                ocspResponseRevoked, willNotRetry);
  add_ocsp_test("ocsp-stapling-ancient-valid.example.com",
                SEC_ERROR_OCSP_UNKNOWN_CERT,
                ocspResponseUnknown, willRetry);

  // Test how OCSP-must-staple (i.e. TLS feature) interacts with stapled OCSP
  // responses that don't successfully verify.
  // A strict reading of the relevant RFCs might say that these connections
  // should all fail because a satisfactory stapled OCSP response is not
  // present, but for compatibility reasons we fall back to active OCSP fetching
  // in these situations. If the fetch succeeds, then connection succeeds.
  add_ocsp_test("ocsp-stapling-must-staple-expired.example.com",
                PRErrorCodeSuccess, ocspResponseGoodMustStaple, willNotRetry);
  add_ocsp_test("ocsp-stapling-must-staple-try-later.example.com",
                PRErrorCodeSuccess, ocspResponseGoodMustStaple, willNotRetry);
  add_ocsp_test("ocsp-stapling-must-staple-invalid-signer.example.com",
                PRErrorCodeSuccess, ocspResponseGoodMustStaple, willNotRetry);

  add_test(function () { ocspResponder.stop(run_next_test); });
  add_test(check_ocsp_stapling_telemetry);
  run_next_test();
}

function check_ocsp_stapling_telemetry() {
  let histogram = Cc["@mozilla.org/base/telemetry;1"]
                    .getService(Ci.nsITelemetry)
                    .getHistogramById("SSL_OCSP_STAPLING")
                    .snapshot();
  equal(histogram.counts[0], 0,
        "Should have 0 connections for unused histogram bucket 0");
  equal(histogram.counts[1], 0,
        "Actual and expected connections with a good response should match");
  equal(histogram.counts[2], 0,
        "Actual and expected connections with no stapled response should match");
  equal(histogram.counts[3], 22,
        "Actual and expected connections with an expired response should match");
  equal(histogram.counts[4], 2,
        "Actual and expected connections with bad responses should match");
  run_next_test();
}
