// -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// In which we connect to a number of domains (as faked by a server running
// locally) with OCSP stapling enabled to determine that good things happen
// and bad things don't, specifically with respect to various expired OCSP
// responses (stapled and otherwise).

let gCurrentOCSPResponse = null;
let gOCSPRequestCount = 0;

function add_ocsp_test(aHost, aExpectedResult, aOCSPResponseToServe) {
  add_connection_test(aHost, aExpectedResult,
    function() {
      clearOCSPCache();
      clearSessionCache();
      gCurrentOCSPResponse = aOCSPResponseToServe;
      gOCSPRequestCount = 0;
    },
    function() {
      do_check_eq(gOCSPRequestCount, 1);
    });
}

do_get_profile();
Services.prefs.setBoolPref("security.ssl.enable_ocsp_stapling", true);
let args = [["good", "localhostAndExampleCom", "unused"],
             ["expiredresponse", "localhostAndExampleCom", "unused"],
             ["oldvalidperiod", "localhostAndExampleCom", "unused"],
             ["revoked", "localhostAndExampleCom", "unused"],
             ["unknown", "localhostAndExampleCom", "unused"],
            ];
let ocspResponses = generateOCSPResponses(args, "tlsserver");
// Fresh response, certificate is good.
let ocspResponseGood = ocspResponses[0];
// Expired response, certificate is good.
let expiredOCSPResponseGood = ocspResponses[1];
// Fresh signature, old validity period, certificate is good.
let oldValidityPeriodOCSPResponseGood = ocspResponses[2];
// Fresh signature, certificate is revoked.
let ocspResponseRevoked = ocspResponses[3];
// Fresh signature, certificate is unknown.
let ocspResponseUnknown = ocspResponses[4];

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
  ocspResponder.start(8080);
  add_tls_server_setup("OCSPStaplingServer");
  add_tests_in_mode(true);
  add_tests_in_mode(false);
  add_test(function () { ocspResponder.stop(run_next_test); });
  add_test(check_ocsp_stapling_telemetry);
  run_next_test();
}

function add_tests_in_mode(useMozillaPKIX)
{
  add_test(function () {
    Services.prefs.setBoolPref("security.use_mozillapkix_verification",
                               useMozillaPKIX);
    run_next_test();
  });

  // In these tests, the OCSP stapling server gives us a stapled
  // response based on the host name ("ocsp-stapling-expired" or
  // "ocsp-stapling-expired-fresh-ca"). We then ensure that we're
  // properly falling back to fetching revocation information.
  // For ocsp-stapling-expired.example.com, the OCSP stapling server
  // staples an expired OCSP response. The certificate has not expired.
  // For ocsp-stapling-expired-fresh-ca.example.com, the OCSP stapling
  // server staples an OCSP response with a recent signature but with an
  // out-of-date validity period. The certificate has not expired.
  add_ocsp_test("ocsp-stapling-expired.example.com", Cr.NS_OK,
                ocspResponseGood);
  add_ocsp_test("ocsp-stapling-expired-fresh-ca.example.com", Cr.NS_OK,
                ocspResponseGood);
  // With mozilla::pkix, if we can't fetch a more recent response when
  // given an expired stapled response, we terminate the connection.
  add_ocsp_test("ocsp-stapling-expired.example.com",
                useMozillaPKIX
                  ? getXPCOMStatusFromNSS(SEC_ERROR_OCSP_OLD_RESPONSE)
                  : Cr.NS_OK,
                expiredOCSPResponseGood);
  add_ocsp_test("ocsp-stapling-expired-fresh-ca.example.com",
                useMozillaPKIX
                  ? getXPCOMStatusFromNSS(SEC_ERROR_OCSP_OLD_RESPONSE)
                  : Cr.NS_OK,
                expiredOCSPResponseGood);
  add_ocsp_test("ocsp-stapling-expired.example.com",
                useMozillaPKIX
                  ? getXPCOMStatusFromNSS(SEC_ERROR_OCSP_OLD_RESPONSE)
                  : Cr.NS_OK,
                oldValidityPeriodOCSPResponseGood);
  add_ocsp_test("ocsp-stapling-expired-fresh-ca.example.com",
                useMozillaPKIX
                  ? getXPCOMStatusFromNSS(SEC_ERROR_OCSP_OLD_RESPONSE)
                  : Cr.NS_OK,
                oldValidityPeriodOCSPResponseGood);
  add_ocsp_test("ocsp-stapling-expired.example.com",
                useMozillaPKIX
                  ? getXPCOMStatusFromNSS(SEC_ERROR_OCSP_OLD_RESPONSE)
                  : Cr.NS_OK,
                null);
  add_ocsp_test("ocsp-stapling-expired.example.com",
                useMozillaPKIX
                  ? getXPCOMStatusFromNSS(SEC_ERROR_OCSP_OLD_RESPONSE)
                  : Cr.NS_OK,
                null);
  // Of course, if the newer response indicates Revoked or Unknown,
  // that status must be returned.
  add_ocsp_test("ocsp-stapling-expired.example.com",
                getXPCOMStatusFromNSS(SEC_ERROR_REVOKED_CERTIFICATE),
                ocspResponseRevoked);
  add_ocsp_test("ocsp-stapling-expired-fresh-ca.example.com",
                getXPCOMStatusFromNSS(SEC_ERROR_REVOKED_CERTIFICATE),
                ocspResponseRevoked);
  add_ocsp_test("ocsp-stapling-expired.example.com",
                getXPCOMStatusFromNSS(SEC_ERROR_OCSP_UNKNOWN_CERT),
                ocspResponseUnknown);
  add_ocsp_test("ocsp-stapling-expired-fresh-ca.example.com",
                getXPCOMStatusFromNSS(SEC_ERROR_OCSP_UNKNOWN_CERT),
                ocspResponseUnknown);

  if (useMozillaPKIX) {
    // If the response is expired but indicates Revoked or Unknown and a
    // newer status can't be fetched, the Revoked or Unknown status will
    // be returned.
    add_ocsp_test("ocsp-stapling-revoked-old.example.com",
                  getXPCOMStatusFromNSS(SEC_ERROR_REVOKED_CERTIFICATE),
                  null);
    add_ocsp_test("ocsp-stapling-unknown-old.example.com",
                  getXPCOMStatusFromNSS(SEC_ERROR_OCSP_UNKNOWN_CERT),
                  null);
    // If the response is expired but indicates Revoked or Unknown and
    // a newer status can be fetched and successfully verified, this
    // should result in a successful certificate verification.
    add_ocsp_test("ocsp-stapling-revoked-old.example.com", Cr.NS_OK,
                  ocspResponseGood);
    add_ocsp_test("ocsp-stapling-unknown-old.example.com", Cr.NS_OK,
                  ocspResponseGood);
    // If a newer status can be fetched but it fails to verify, the
    // Revoked or Unknown status of the expired stapled response
    // should be returned.
    add_ocsp_test("ocsp-stapling-revoked-old.example.com",
                  getXPCOMStatusFromNSS(SEC_ERROR_REVOKED_CERTIFICATE),
                  expiredOCSPResponseGood);
    add_ocsp_test("ocsp-stapling-unknown-old.example.com",
                  getXPCOMStatusFromNSS(SEC_ERROR_OCSP_UNKNOWN_CERT),
                  expiredOCSPResponseGood);
  }

  if (useMozillaPKIX) {
    // These tests are verifying that an valid but very old response
    // is rejected as a valid stapled response, requiring a fetch
    // from the ocsp responder.
    add_ocsp_test("ocsp-stapling-ancient-valid.example.com", Cr.NS_OK,
                  ocspResponseGood);
    add_ocsp_test("ocsp-stapling-ancient-valid.example.com",
                  getXPCOMStatusFromNSS(SEC_ERROR_REVOKED_CERTIFICATE),
                  ocspResponseRevoked);
    add_ocsp_test("ocsp-stapling-ancient-valid.example.com",
                  getXPCOMStatusFromNSS(SEC_ERROR_OCSP_UNKNOWN_CERT),
                  ocspResponseUnknown);
  }
}

function check_ocsp_stapling_telemetry() {
  let histogram = Cc["@mozilla.org/base/telemetry;1"]
                    .getService(Ci.nsITelemetry)
                    .getHistogramById("SSL_OCSP_STAPLING")
                    .snapshot();
  do_check_eq(histogram.counts[0], 2 * 0); // histogram bucket 0 is unused
  do_check_eq(histogram.counts[1], 2 * 0); // 0 connections with a good response
  do_check_eq(histogram.counts[2], 2 * 0); // 0 connections with no stapled resp.
  do_check_eq(histogram.counts[3], 2 * 12 + 9); // 12 connections with an expired response
                                                // +9 more mozilla::pkix-only expired responses
  do_check_eq(histogram.counts[4], 2 * 0); // 0 connections with bad responses
  run_next_test();
}
