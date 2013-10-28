// -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// In which we connect to a number of domains (as faked by a server running
// locally) with and without OCSP stapling enabled to determine that good
// things happen and bad things don't.

function add_ocsp_test(aHost, aExpectedResult, aStaplingEnabled) {
  add_connection_test(aHost, aExpectedResult,
    function() {
      clearOCSPCache();
      Services.prefs.setBoolPref("security.ssl.enable_ocsp_stapling",
                                 aStaplingEnabled);
    });
}

function run_test() {
  do_get_profile();

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

  // SEC_ERROR_REVOKED_CERTIFICATE = SEC_ERROR_BASE + 12
  add_ocsp_test("ocsp-stapling-revoked.example.com", getXPCOMStatusFromNSS(12), true);

  // This stapled response is from a CA that is untrusted and did not issue
  // the server's certificate.
  // SEC_ERROR_BAD_DATABASE = SEC_ERROR_BASE + 18
  add_ocsp_test("ocsp-stapling-good-other-ca.example.com", getXPCOMStatusFromNSS(18), true);

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

  // SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE = (SEC_ERROR_BASE + 130)
  add_ocsp_test("ocsp-stapling-good-other-ca.example.com", getXPCOMStatusFromNSS(130), true);
  // SEC_ERROR_OCSP_MALFORMED_REQUEST = (SEC_ERROR_BASE + 120)
  add_ocsp_test("ocsp-stapling-malformed.example.com", getXPCOMStatusFromNSS(120), true);
  // SEC_ERROR_OCSP_SERVER_ERROR = (SEC_ERROR_BASE + 121)
  add_ocsp_test("ocsp-stapling-srverr.example.com", getXPCOMStatusFromNSS(121), true);
  // SEC_ERROR_OCSP_TRY_SERVER_LATER = (SEC_ERROR_BASE + 122)
  add_ocsp_test("ocsp-stapling-trylater.example.com", getXPCOMStatusFromNSS(122), true);
  // SEC_ERROR_OCSP_REQUEST_NEEDS_SIG = (SEC_ERROR_BASE + 123)
  add_ocsp_test("ocsp-stapling-needssig.example.com", getXPCOMStatusFromNSS(123), true);
  // SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST = (SEC_ERROR_BASE + 124)
  add_ocsp_test("ocsp-stapling-unauthorized.example.com", getXPCOMStatusFromNSS(124), true);
  // SEC_ERROR_OCSP_UNKNOWN_CERT = (SEC_ERROR_BASE + 126)
  add_ocsp_test("ocsp-stapling-unknown.example.com", getXPCOMStatusFromNSS(126), true);
  add_ocsp_test("ocsp-stapling-good-other.example.com", getXPCOMStatusFromNSS(126), true);
  // If the server doesn't send an OCSP response, we continue as normal.
  add_ocsp_test("ocsp-stapling-none.example.com", Cr.NS_OK, true);
  // SEC_ERROR_OCSP_MALFORMED_RESPONSE = (SEC_ERROR_BASE + 129)
  add_ocsp_test("ocsp-stapling-empty.example.com", getXPCOMStatusFromNSS(129), true);
  // SEC_ERROR_OCSP_OLD_RESPONSE = (SEC_ERROR_BASE + 132)
  add_ocsp_test("ocsp-stapling-expired.example.com", getXPCOMStatusFromNSS(132), true);
  add_ocsp_test("ocsp-stapling-expired-fresh-ca.example.com", getXPCOMStatusFromNSS(132), true);

  run_next_test();
}
