// -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// Tests the certificate overrides we allow.
// add_cert_override_test will queue a test that does the following:
// 1. Attempt to connect to the given host. This should fail with the
//    given error and override bits.
// 2. Add an override for that host/port/certificate/override bits.
// 3. Connect again. This should succeed.

function add_cert_override(aHost, aExpectedBits, aSecurityInfo) {
  let sslstatus = aSecurityInfo.QueryInterface(Ci.nsISSLStatusProvider)
                               .SSLStatus;
  let bits =
    (sslstatus.isUntrusted ? Ci.nsICertOverrideService.ERROR_UNTRUSTED : 0) |
    (sslstatus.isDomainMismatch ? Ci.nsICertOverrideService.ERROR_MISMATCH : 0) |
    (sslstatus.isNotValidAtThisTime ? Ci.nsICertOverrideService.ERROR_TIME : 0);
  do_check_eq(bits, aExpectedBits);
  let cert = sslstatus.serverCert;
  let certOverrideService = Cc["@mozilla.org/security/certoverride;1"]
                              .getService(Ci.nsICertOverrideService);
  certOverrideService.rememberValidityOverride(aHost, 8443, cert, aExpectedBits,
                                               true);
}

function add_cert_override_test(aHost, aExpectedBits, aExpectedError) {
  add_connection_test(aHost, aExpectedError, null,
                      add_cert_override.bind(this, aHost, aExpectedBits));
  add_connection_test(aHost, Cr.NS_OK);
}

function check_telemetry() {
  let histogram = Cc["@mozilla.org/base/telemetry;1"]
                    .getService(Ci.nsITelemetry)
                    .getHistogramById("SSL_CERT_ERROR_OVERRIDES")
                    .snapshot();
  do_check_eq(histogram.counts[ 0], 0);
  do_check_eq(histogram.counts[ 1], 1);
  do_check_eq(histogram.counts[ 2], 1);
  do_check_eq(histogram.counts[ 3], 1);
  do_check_eq(histogram.counts[ 4], 1);
  do_check_eq(histogram.counts[ 5], 1);
  do_check_eq(histogram.counts[ 6], 1);
  do_check_eq(histogram.counts[ 7], 1);
  do_check_eq(histogram.counts[ 8], 1);
  do_check_eq(histogram.counts[ 9], 1);
  do_check_eq(histogram.counts[10], 1);
  run_next_test();
}

function run_test() {
  do_get_profile();
  add_tls_server_setup("BadCertServer");
  add_cert_override_test("expired.example.com",
                         Ci.nsICertOverrideService.ERROR_TIME,
                         getXPCOMStatusFromNSS(SEC_ERROR_EXPIRED_CERTIFICATE));
  add_cert_override_test("selfsigned.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         getXPCOMStatusFromNSS(SEC_ERROR_CA_CERT_INVALID));
  add_cert_override_test("unknownissuer.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         getXPCOMStatusFromNSS(SEC_ERROR_UNKNOWN_ISSUER));
  add_cert_override_test("expiredissuer.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         getXPCOMStatusFromNSS(SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE));
  add_cert_override_test("md5signature.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         getXPCOMStatusFromNSS(SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED));
  add_cert_override_test("inadequatekeyusage.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         getXPCOMStatusFromNSS(SEC_ERROR_INADEQUATE_KEY_USAGE));
  add_cert_override_test("mismatch.example.com",
                         Ci.nsICertOverrideService.ERROR_MISMATCH,
                         getXPCOMStatusFromNSS(SSL_ERROR_BAD_CERT_DOMAIN));
  // Before we specifically distrust this certificate, it should be trusted.
  add_connection_test("untrusted.example.com", Cr.NS_OK);
  add_test(function() {
    let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                    .getService(Ci.nsIX509CertDB);
    // A trust argument of "pu" means the cert is actively distrusted.
    addCertFromFile(certdb, "tlsserver/default-ee.der", "pu,,");
    clearSessionCache();
    run_next_test();
  });
  add_cert_override_test("untrusted.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         getXPCOMStatusFromNSS(SEC_ERROR_UNTRUSTED_CERT));
  add_test(function() {
    let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                    .getService(Ci.nsIX509CertDB);
    // A trust argument of "pu" means the cert is actively distrusted.
    addCertFromFile(certdb, "tlsserver/other-test-ca.der", "pu,,");
    run_next_test();
  });
  add_cert_override_test("untrustedissuer.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         getXPCOMStatusFromNSS(SEC_ERROR_UNTRUSTED_ISSUER));
  add_test(check_telemetry);
  run_next_test();
}
