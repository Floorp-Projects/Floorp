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

do_get_profile();
let certOverrideService = Cc["@mozilla.org/security/certoverride;1"]
                            .getService(Ci.nsICertOverrideService);

function add_cert_override(aHost, aExpectedBits, aSecurityInfo) {
  let sslstatus = aSecurityInfo.QueryInterface(Ci.nsISSLStatusProvider)
                               .SSLStatus;
  let bits =
    (sslstatus.isUntrusted ? Ci.nsICertOverrideService.ERROR_UNTRUSTED : 0) |
    (sslstatus.isDomainMismatch ? Ci.nsICertOverrideService.ERROR_MISMATCH : 0) |
    (sslstatus.isNotValidAtThisTime ? Ci.nsICertOverrideService.ERROR_TIME : 0);
  do_check_eq(bits, aExpectedBits);
  let cert = sslstatus.serverCert;
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
  do_check_eq(histogram.counts[ 2], 7 + 1); // SEC_ERROR_UNKNOWN_ISSUER
  do_check_eq(histogram.counts[ 3], 0 + 2); // SEC_ERROR_CA_CERT_INVALID
  do_check_eq(histogram.counts[ 4], 0 + 4); // SEC_ERROR_UNTRUSTED_ISSUER
  do_check_eq(histogram.counts[ 5], 0 + 1); // SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE
  do_check_eq(histogram.counts[ 6], 0 + 1); // SEC_ERROR_UNTRUSTED_CERT
  do_check_eq(histogram.counts[ 7], 0 + 1); // SEC_ERROR_INADEQUATE_KEY_USAGE
  do_check_eq(histogram.counts[ 8], 2 + 2); // SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED
  do_check_eq(histogram.counts[ 9], 4 + 4); // SSL_ERROR_BAD_CERT_DOMAIN
  do_check_eq(histogram.counts[10], 5 + 5); // SEC_ERROR_EXPIRED_CERTIFICATE

  run_next_test();
}

function run_test() {
  add_tls_server_setup("BadCertServer");

  let fakeOCSPResponder = new HttpServer();
  fakeOCSPResponder.registerPrefixHandler("/", function (request, response) {
    response.setStatusLine(request.httpVersion, 500, "Internal Server Error");
  });
  fakeOCSPResponder.start(8080);

  add_tests_in_mode(true);
  add_tests_in_mode(false);

  add_test(function () {
    fakeOCSPResponder.stop(check_telemetry);
  });

  run_next_test();
}

function add_tests_in_mode(useMozillaPKIX) {
  add_test(function () {
    Services.prefs.setBoolPref("security.use_mozillapkix_verification",
                               useMozillaPKIX);
    run_next_test();
  });

  add_simple_tests(useMozillaPKIX);
  add_combo_tests(useMozillaPKIX);
  add_distrust_tests(useMozillaPKIX);

  add_test(function () {
    certOverrideService.clearValidityOverride("all:temporary-certificates", 0);
    run_next_test();
  });
}

function add_simple_tests(useMozillaPKIX) {
  add_cert_override_test("expired.example.com",
                         Ci.nsICertOverrideService.ERROR_TIME,
                         getXPCOMStatusFromNSS(SEC_ERROR_EXPIRED_CERTIFICATE));
  add_cert_override_test("selfsigned.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         getXPCOMStatusFromNSS(
                            useMozillaPKIX ? SEC_ERROR_UNKNOWN_ISSUER
                                           : SEC_ERROR_CA_CERT_INVALID));
  add_cert_override_test("unknownissuer.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         getXPCOMStatusFromNSS(SEC_ERROR_UNKNOWN_ISSUER));
  add_cert_override_test("expiredissuer.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         getXPCOMStatusFromNSS(
                            useMozillaPKIX ? SEC_ERROR_UNKNOWN_ISSUER
                                           : SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE));
  add_cert_override_test("md5signature.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         getXPCOMStatusFromNSS(
                            SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED));
  add_cert_override_test("mismatch.example.com",
                         Ci.nsICertOverrideService.ERROR_MISMATCH,
                         getXPCOMStatusFromNSS(SSL_ERROR_BAD_CERT_DOMAIN));

  // A Microsoft IIS utility generates self-signed certificates with
  // properties similar to the one this "host" will present (see
  // tlsserver/generate_certs.sh).
  // One of the errors classic verification collects is that this
  // certificate has an inadequate key usage to sign a certificate
  // (i.e. itself). As a result, to be able to override this,
  // SEC_ERROR_INADEQUATE_KEY_USAGE must be overridable (although,
  // confusingly, this isn't the main error reported).
  // mozilla::pkix just says this certificate's issuer is unknown.
  add_cert_override_test("selfsigned-inadequateEKU.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         getXPCOMStatusFromNSS(
                            useMozillaPKIX ? SEC_ERROR_UNKNOWN_ISSUER
                                           : SEC_ERROR_CA_CERT_INVALID));

  // SEC_ERROR_INADEQUATE_KEY_USAGE is overridable in general for
  // classic verification, but not for mozilla::pkix verification.
  if (useMozillaPKIX) {
    add_connection_test("inadequatekeyusage.example.com",
                        getXPCOMStatusFromNSS(SEC_ERROR_INADEQUATE_KEY_USAGE),
                        null,
                        function (securityInfo) {
                          // bug 754369 - no SSLStatus probably means this is
                          // a non-overridable error, which is what we're testing
                          // (although it would be best to test this directly).
                          securityInfo.QueryInterface(Ci.nsISSLStatusProvider);
                          do_check_eq(securityInfo.SSLStatus, null);
                        });
  } else {
    add_cert_override_test("inadequatekeyusage.example.com",
                           Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                           getXPCOMStatusFromNSS(SEC_ERROR_INADEQUATE_KEY_USAGE));
  }
}

function add_combo_tests(useMozillaPKIX) {
  // Note that "untrusted" here really is "unknown issuer" in the
  // mozilla::pkix case.

  add_cert_override_test("mismatch-expired.example.com",
                         Ci.nsICertOverrideService.ERROR_MISMATCH |
                         Ci.nsICertOverrideService.ERROR_TIME,
                         getXPCOMStatusFromNSS(SSL_ERROR_BAD_CERT_DOMAIN));
  add_cert_override_test("mismatch-untrusted.example.com",
                         Ci.nsICertOverrideService.ERROR_MISMATCH |
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         getXPCOMStatusFromNSS(
                            useMozillaPKIX ? SEC_ERROR_UNKNOWN_ISSUER
                                           : SEC_ERROR_UNTRUSTED_ISSUER));
  add_cert_override_test("untrusted-expired.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                         Ci.nsICertOverrideService.ERROR_TIME,
                         getXPCOMStatusFromNSS(
                            useMozillaPKIX ? SEC_ERROR_UNKNOWN_ISSUER
                                           : SEC_ERROR_UNTRUSTED_ISSUER));
  add_cert_override_test("mismatch-untrusted-expired.example.com",
                         Ci.nsICertOverrideService.ERROR_MISMATCH |
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                         Ci.nsICertOverrideService.ERROR_TIME,
                         getXPCOMStatusFromNSS(
                            useMozillaPKIX ? SEC_ERROR_UNKNOWN_ISSUER
                                           : SEC_ERROR_UNTRUSTED_ISSUER));

  add_cert_override_test("md5signature-expired.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                         Ci.nsICertOverrideService.ERROR_TIME,
                         getXPCOMStatusFromNSS(
                            SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED));
}

function add_distrust_tests(useMozillaPKIX) {
  // Before we specifically distrust this certificate, it should be trusted.
  add_connection_test("untrusted.example.com", Cr.NS_OK);

  // XXX(Bug 975777): Active distrust is an overridable error when NSS-based
  // verification is used.
  add_distrust_override_test("tlsserver/default-ee.der",
                             "untrusted.example.com",
                             getXPCOMStatusFromNSS(SEC_ERROR_UNTRUSTED_CERT),
                             useMozillaPKIX
                                ? getXPCOMStatusFromNSS(SEC_ERROR_UNTRUSTED_CERT)
                                : Cr.NS_OK);

  // XXX(Bug 975777): Active distrust is an overridable error when NSS-based
  // verification is used.
  add_distrust_override_test("tlsserver/other-test-ca.der",
                             "untrustedissuer.example.com",
                             getXPCOMStatusFromNSS(SEC_ERROR_UNTRUSTED_ISSUER),
                             useMozillaPKIX
                                ? getXPCOMStatusFromNSS(SEC_ERROR_UNTRUSTED_ISSUER)
                                : Cr.NS_OK);
}

function add_distrust_override_test(certFileName, hostName,
                                    expectedResultBefore, expectedResultAfter) {
  let certToDistrust = constructCertFromFile(certFileName);

  add_test(function () {
    // "pu" means the cert is actively distrusted.
    setCertTrust(certToDistrust, "pu,,");
    clearSessionCache();
    run_next_test();
  });
  add_connection_test(hostName, expectedResultBefore, null,
                      function (securityInfo) {
                        securityInfo.QueryInterface(Ci.nsISSLStatusProvider);
                        // XXX(Bug 754369): SSLStatus isn't available for
                        // non-overridable errors.
                        if (securityInfo.SSLStatus) {
                          certOverrideService.rememberValidityOverride(
                              hostName, 8443, securityInfo.SSLStatus.serverCert,
                              Ci.nsICertOverrideService.ERROR_UNTRUSTED, true);
                        } else {
                          // A missing SSLStatus probably means (due to bug
                          // 754369) that the error was non-overridable, which
                          // is what we're trying to test, though we'd rather
                          // not test it this way.
                          do_check_neq(expectedResultAfter, Cr.NS_OK);
                        }
                        clearSessionCache();
                      });
  add_connection_test(hostName, expectedResultAfter, null,
                      function () {
                        setCertTrust(certToDistrust, "u,,");
                      });
}
