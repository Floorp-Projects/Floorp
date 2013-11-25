// -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// In which we connect to a number of domains (as faked by a server running
// locally) with and without OCSP stapling enabled to determine that good
// things happen and bad things don't.

let gExpectOCSPRequest;

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

function run_test() {
  do_get_profile();

  let fakeOCSPResponder = new HttpServer();
  fakeOCSPResponder.registerPrefixHandler("/", function(request, response) {
    response.setStatusLine(request.httpVersion, 500, "Internal Server Error");
    do_check_true(gExpectOCSPRequest);
  });
  fakeOCSPResponder.start(8080);

  add_tls_server_setup("OCSPStaplingServer");

  // In the absence of OCSP stapling, these should actually all work.
  add_ocsp_test("ocsp-stapling-good.example.com", Cr.NS_OK, false);
  add_ocsp_test("ocsp-stapling-revoked.example.com", Cr.NS_OK, false);
  add_ocsp_test("ocsp-stapling-good-other-ca.example.com", Cr.NS_OK, false);
  add_ocsp_test("ocsp-stapling-malformed.example.com", Cr.NS_OK, false);
  add_ocsp_test("ocsp-stapling-srverr.example.com", Cr.NS_OK, false);
  add_ocsp_test("ocsp-stapling-trylater.example.com", Cr.NS_OK, false);
  add_ocsp_test("ocsp-stapling-needssig.example.com", Cr.NS_OK, false);
  add_ocsp_test("ocsp-stapling-unauthorized.example.com", Cr.NS_OK, false);
  add_ocsp_test("ocsp-stapling-unknown.example.com", Cr.NS_OK, false);
  add_ocsp_test("ocsp-stapling-good-other.example.com", Cr.NS_OK, false);
  add_ocsp_test("ocsp-stapling-none.example.com", Cr.NS_OK, false);
  add_ocsp_test("ocsp-stapling-expired.example.com", Cr.NS_OK, false);
  add_ocsp_test("ocsp-stapling-expired-fresh-ca.example.com", Cr.NS_OK, false);

  // Now test OCSP stapling
  // The following error codes are defined in security/nss/lib/util/SECerrs.h

  add_ocsp_test("ocsp-stapling-good.example.com", Cr.NS_OK, true);

  add_ocsp_test("ocsp-stapling-revoked.example.com",
                getXPCOMStatusFromNSS(SEC_ERROR_REVOKED_CERTIFICATE), true);

  // This stapled response is from a CA that is untrusted and did not issue
  // the server's certificate.
  add_ocsp_test("ocsp-stapling-good-other-ca.example.com",
                getXPCOMStatusFromNSS(SEC_ERROR_BAD_DATABASE), true);

  // SEC_ERROR_BAD_DATABASE vs SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE depends on
  // whether the CA that signed the response is a trusted CA.
  add_test(function() {
    let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                   .getService(Ci.nsIX509CertDB);
    // Another trusted CA that shouldn't be trusted for OCSP responses, etc.
    // for the "good" CA.
    addCertFromFile(certdb, "tlsserver/other-test-ca.der", "CTu,u,u");
    run_next_test();
  });

  add_ocsp_test("ocsp-stapling-good-other-ca.example.com",
                getXPCOMStatusFromNSS(SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE),
                true);
  add_ocsp_test("ocsp-stapling-malformed.example.com",
                getXPCOMStatusFromNSS(SEC_ERROR_OCSP_MALFORMED_REQUEST), true);
  add_ocsp_test("ocsp-stapling-srverr.example.com",
                getXPCOMStatusFromNSS(SEC_ERROR_OCSP_SERVER_ERROR), true);
  add_ocsp_test("ocsp-stapling-trylater.example.com",
                getXPCOMStatusFromNSS(SEC_ERROR_OCSP_TRY_SERVER_LATER), true);
  add_ocsp_test("ocsp-stapling-needssig.example.com",
                getXPCOMStatusFromNSS(SEC_ERROR_OCSP_REQUEST_NEEDS_SIG), true);
  add_ocsp_test("ocsp-stapling-unauthorized.example.com",
                getXPCOMStatusFromNSS(SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST),
                true);
  add_ocsp_test("ocsp-stapling-unknown.example.com",
                getXPCOMStatusFromNSS(SEC_ERROR_OCSP_UNKNOWN_CERT), true);
  add_ocsp_test("ocsp-stapling-good-other.example.com",
                getXPCOMStatusFromNSS(SEC_ERROR_OCSP_UNKNOWN_CERT), true);
  // If the server doesn't staple an OCSP response, we continue as normal
  // (this means that even though stapling is enabled, we expect an OCSP
  // request).
  add_connection_test("ocsp-stapling-none.example.com", Cr.NS_OK,
    function() {
      gExpectOCSPRequest = true;
      clearOCSPCache();
      clearSessionCache();
      Services.prefs.setBoolPref("security.ssl.enable_ocsp_stapling", true);
    }
  );
  add_ocsp_test("ocsp-stapling-empty.example.com",
                getXPCOMStatusFromNSS(SEC_ERROR_OCSP_MALFORMED_RESPONSE), true);
  // ocsp-stapling-expired.example.com and
  // ocsp-stapling-expired-fresh-ca.example.com are handled in
  // test_ocsp_stapling_expired.js

  add_test(function() { fakeOCSPResponder.stop(run_next_test); });
  run_next_test();
}
