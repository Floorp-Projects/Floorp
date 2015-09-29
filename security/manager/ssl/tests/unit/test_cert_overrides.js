// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
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

function check_telemetry() {
  let histogram = Cc["@mozilla.org/base/telemetry;1"]
                    .getService(Ci.nsITelemetry)
                    .getHistogramById("SSL_CERT_ERROR_OVERRIDES")
                    .snapshot();
  equal(histogram.counts[ 0], 0, "Should have 0 unclassified counts");
  equal(histogram.counts[ 2], 7,
        "Actual and expected SEC_ERROR_UNKNOWN_ISSUER counts should match");
  equal(histogram.counts[ 3], 1,
        "Actual and expected SEC_ERROR_CA_CERT_INVALID counts should match");
  equal(histogram.counts[ 4], 0,
        "Actual and expected SEC_ERROR_UNTRUSTED_ISSUER counts should match");
  equal(histogram.counts[ 5], 1,
        "Actual and expected SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE counts should match");
  equal(histogram.counts[ 6], 0,
        "Actual and expected SEC_ERROR_UNTRUSTED_CERT counts should match");
  equal(histogram.counts[ 7], 0,
        "Actual and expected SEC_ERROR_INADEQUATE_KEY_USAGE counts should match");
  equal(histogram.counts[ 8], 2,
        "Actual and expected SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED counts should match");
  equal(histogram.counts[ 9], 10,
        "Actual and expected SSL_ERROR_BAD_CERT_DOMAIN counts should match");
  equal(histogram.counts[10], 5,
        "Actual and expected SEC_ERROR_EXPIRED_CERTIFICATE counts should match");
  equal(histogram.counts[11], 2,
        "Actual and expected MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY counts should match");
  equal(histogram.counts[12], 1,
        "Actual and expected MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA counts should match");
  equal(histogram.counts[13], 0,
        "Actual and expected MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE counts should match");
  equal(histogram.counts[14], 2,
        "Actual and expected MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE counts should match");
  equal(histogram.counts[15], 1,
        "Actual and expected MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE counts should match");
  equal(histogram.counts[16], 2,
        "Actual and expected SEC_ERROR_INVALID_TIME counts should match");

  let keySizeHistogram = Cc["@mozilla.org/base/telemetry;1"]
                           .getService(Ci.nsITelemetry)
                           .getHistogramById("CERT_CHAIN_KEY_SIZE_STATUS")
                           .snapshot();
  equal(keySizeHistogram.counts[0], 0,
        "Actual and expected unchecked key size counts should match");
  equal(keySizeHistogram.counts[1], 12,
        "Actual and expected successful verifications of 2048-bit keys should match");
  equal(keySizeHistogram.counts[2], 0,
        "Actual and expected successful verifications of 1024-bit keys should match");
  equal(keySizeHistogram.counts[3], 54,
        "Actual and expected key size verification failures should match");

  run_next_test();
}

function run_test() {
  Services.prefs.setIntPref("security.OCSP.enabled", 1);
  add_tls_server_setup("BadCertServer", "bad_certs");

  let fakeOCSPResponder = new HttpServer();
  fakeOCSPResponder.registerPrefixHandler("/", function (request, response) {
    response.setStatusLine(request.httpVersion, 500, "Internal Server Error");
  });
  fakeOCSPResponder.start(8888);

  add_simple_tests();
  add_combo_tests();
  add_distrust_tests();

  add_test(function () {
    fakeOCSPResponder.stop(check_telemetry);
  });

  run_next_test();
}

function add_simple_tests() {
  add_cert_override_test("expired.example.com",
                         Ci.nsICertOverrideService.ERROR_TIME,
                         SEC_ERROR_EXPIRED_CERTIFICATE);
  add_cert_override_test("notyetvalid.example.com",
                         Ci.nsICertOverrideService.ERROR_TIME,
                         MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE);
  add_cert_override_test("before-epoch.example.com",
                         Ci.nsICertOverrideService.ERROR_TIME,
                         SEC_ERROR_INVALID_TIME);
  add_cert_override_test("selfsigned.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         SEC_ERROR_UNKNOWN_ISSUER);
  add_cert_override_test("unknownissuer.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         SEC_ERROR_UNKNOWN_ISSUER);
  add_cert_override_test("expiredissuer.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE);
  add_cert_override_test("notyetvalidissuer.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE);
  add_cert_override_test("before-epoch-issuer.example.com",
                         Ci.nsICertOverrideService.ERROR_TIME,
                         SEC_ERROR_INVALID_TIME);
  add_cert_override_test("md5signature.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);
  // This has name information in the subject alternative names extension,
  // but not the subject common name.
  add_cert_override_test("mismatch.example.com",
                         Ci.nsICertOverrideService.ERROR_MISMATCH,
                         SSL_ERROR_BAD_CERT_DOMAIN);
  // This has name information in the subject common name but not the subject
  // alternative names extension.
  add_cert_override_test("mismatch-CN.example.com",
                         Ci.nsICertOverrideService.ERROR_MISMATCH,
                         SSL_ERROR_BAD_CERT_DOMAIN);

  // A Microsoft IIS utility generates self-signed certificates with
  // properties similar to the one this "host" will present.
  add_cert_override_test("selfsigned-inadequateEKU.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         SEC_ERROR_UNKNOWN_ISSUER);

  add_prevented_cert_override_test("inadequatekeyusage.example.com",
                                   Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                                   SEC_ERROR_INADEQUATE_KEY_USAGE);

  // This is intended to test the case where a verification has failed for one
  // overridable reason (e.g. unknown issuer) but then, in the process of
  // reporting that error, a non-overridable error is encountered. The
  // non-overridable error should be prioritized.
  add_test(function() {
    let rootCert = constructCertFromFile("bad_certs/test-ca.pem");
    setCertTrust(rootCert, ",,");
    run_next_test();
  });
  add_prevented_cert_override_test("nsCertTypeCritical.example.com",
                                   Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                                   SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION);
  add_test(function() {
    let rootCert = constructCertFromFile("bad_certs/test-ca.pem");
    setCertTrust(rootCert, "CTu,,");
    run_next_test();
  });

  // Bug 990603: Apache documentation has recommended generating a self-signed
  // test certificate with basic constraints: CA:true. For compatibility, this
  // is a scenario in which an override is allowed.
  add_cert_override_test("self-signed-end-entity-with-cA-true.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         SEC_ERROR_UNKNOWN_ISSUER);

  add_cert_override_test("ca-used-as-end-entity.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY);

  // If an X.509 version 1 certificate is not a trust anchor, we will
  // encounter an overridable error.
  add_cert_override_test("end-entity-issued-by-v1-cert.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA);
  // If we make that certificate a trust anchor, the connection will succeed.
  add_test(function() {
    let certOverrideService = Cc["@mozilla.org/security/certoverride;1"]
                                .getService(Ci.nsICertOverrideService);
    certOverrideService.clearValidityOverride("end-entity-issued-by-v1-cert.example.com", 8443);
    let v1Cert = constructCertFromFile("bad_certs/v1Cert.pem");
    setCertTrust(v1Cert, "CTu,,");
    clearSessionCache();
    run_next_test();
  });
  add_connection_test("end-entity-issued-by-v1-cert.example.com",
                      PRErrorCodeSuccess);
  // Reset the trust for that certificate.
  add_test(function() {
    let v1Cert = constructCertFromFile("bad_certs/v1Cert.pem");
    setCertTrust(v1Cert, ",,");
    clearSessionCache();
    run_next_test();
  });

  // Due to compatibility issues, we allow overrides for certificates issued by
  // certificates that are not valid CAs.
  add_cert_override_test("end-entity-issued-by-non-CA.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         SEC_ERROR_CA_CERT_INVALID);

  // This host presents a 1016-bit RSA key. NSS determines this key is too
  // small and terminates the connection. The error is not overridable.
  add_prevented_cert_override_test("inadequate-key-size-ee.example.com",
                                   Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                                   SSL_ERROR_WEAK_SERVER_CERT_KEY);

  add_cert_override_test("ipAddressAsDNSNameInSAN.example.com",
                         Ci.nsICertOverrideService.ERROR_MISMATCH,
                         SSL_ERROR_BAD_CERT_DOMAIN);
  add_cert_override_test("noValidNames.example.com",
                         Ci.nsICertOverrideService.ERROR_MISMATCH,
                         SSL_ERROR_BAD_CERT_DOMAIN);
  add_cert_override_test("badSubjectAltNames.example.com",
                         Ci.nsICertOverrideService.ERROR_MISMATCH,
                         SSL_ERROR_BAD_CERT_DOMAIN);
}

function add_combo_tests() {
  add_cert_override_test("mismatch-expired.example.com",
                         Ci.nsICertOverrideService.ERROR_MISMATCH |
                         Ci.nsICertOverrideService.ERROR_TIME,
                         SSL_ERROR_BAD_CERT_DOMAIN);
  add_cert_override_test("mismatch-notYetValid.example.com",
                         Ci.nsICertOverrideService.ERROR_MISMATCH |
                         Ci.nsICertOverrideService.ERROR_TIME,
                         SSL_ERROR_BAD_CERT_DOMAIN);
  add_cert_override_test("mismatch-untrusted.example.com",
                         Ci.nsICertOverrideService.ERROR_MISMATCH |
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         SEC_ERROR_UNKNOWN_ISSUER);
  add_cert_override_test("untrusted-expired.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                         Ci.nsICertOverrideService.ERROR_TIME,
                         SEC_ERROR_UNKNOWN_ISSUER);
  add_cert_override_test("mismatch-untrusted-expired.example.com",
                         Ci.nsICertOverrideService.ERROR_MISMATCH |
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                         Ci.nsICertOverrideService.ERROR_TIME,
                         SEC_ERROR_UNKNOWN_ISSUER);

  add_cert_override_test("md5signature-expired.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                         Ci.nsICertOverrideService.ERROR_TIME,
                         SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);

  add_cert_override_test("ca-used-as-end-entity-name-mismatch.example.com",
                         Ci.nsICertOverrideService.ERROR_MISMATCH |
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY);
}

function add_distrust_tests() {
  // Before we specifically distrust this certificate, it should be trusted.
  add_connection_test("untrusted.example.com", PRErrorCodeSuccess);

  add_distrust_test("bad_certs/default-ee.pem", "untrusted.example.com",
                    SEC_ERROR_UNTRUSTED_CERT);

  add_distrust_test("bad_certs/other-test-ca.pem",
                    "untrustedissuer.example.com", SEC_ERROR_UNTRUSTED_ISSUER);

  add_distrust_test("bad_certs/test-ca.pem",
                    "ca-used-as-end-entity.example.com",
                    SEC_ERROR_UNTRUSTED_ISSUER);
}

function add_distrust_test(certFileName, hostName, expectedResult) {
  let certToDistrust = constructCertFromFile(certFileName);

  add_test(function () {
    // Add an entry to the NSS certDB that says to distrust the cert
    setCertTrust(certToDistrust, "pu,,");
    clearSessionCache();
    run_next_test();
  });
  add_prevented_cert_override_test(hostName,
                                   Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                                   expectedResult);
  add_test(function () {
    setCertTrust(certToDistrust, "u,,");
    run_next_test();
  });
}
