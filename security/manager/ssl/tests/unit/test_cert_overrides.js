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
  let histogram = Services.telemetry
    .getHistogramById("SSL_CERT_ERROR_OVERRIDES")
    .snapshot();
  equal(histogram.values[0], 0, "Should have 0 unclassified values");
  equal(
    histogram.values[2],
    9,
    "Actual and expected SEC_ERROR_UNKNOWN_ISSUER values should match"
  );
  equal(
    histogram.values[3],
    1,
    "Actual and expected SEC_ERROR_CA_CERT_INVALID values should match"
  );
  equal(
    histogram.values[4] || 0,
    0,
    "Actual and expected SEC_ERROR_UNTRUSTED_ISSUER values should match"
  );
  equal(
    histogram.values[5],
    1,
    "Actual and expected SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE values should match"
  );
  equal(
    histogram.values[6] || 0,
    0,
    "Actual and expected SEC_ERROR_UNTRUSTED_CERT values should match"
  );
  equal(
    histogram.values[7] || 0,
    0,
    "Actual and expected SEC_ERROR_INADEQUATE_KEY_USAGE values should match"
  );
  equal(
    histogram.values[8],
    2,
    "Actual and expected SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED values should match"
  );
  equal(
    histogram.values[9],
    13,
    "Actual and expected SSL_ERROR_BAD_CERT_DOMAIN values should match"
  );
  equal(
    histogram.values[10],
    5,
    "Actual and expected SEC_ERROR_EXPIRED_CERTIFICATE values should match"
  );
  equal(
    histogram.values[11],
    2,
    "Actual and expected MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY values should match"
  );
  equal(
    histogram.values[12],
    1,
    "Actual and expected MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA values should match"
  );
  equal(
    histogram.values[13],
    1,
    "Actual and expected MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE values should match"
  );
  equal(
    histogram.values[14],
    2,
    "Actual and expected MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE values should match"
  );
  equal(
    histogram.values[15],
    1,
    "Actual and expected MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE values should match"
  );
  equal(
    histogram.values[16],
    2,
    "Actual and expected SEC_ERROR_INVALID_TIME values should match"
  );
  equal(
    histogram.values[17],
    1,
    "Actual and expected MOZILLA_PKIX_ERROR_EMPTY_ISSUER_NAME values should match"
  );
  equal(
    histogram.values[19],
    3,
    "Actual and expected MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT values should match"
  );
  equal(
    histogram.values[20],
    1,
    "Actual and expected MOZILLA_PKIX_ERROR_MITM_DETECTED values should match"
  );

  let keySizeHistogram = Services.telemetry
    .getHistogramById("CERT_CHAIN_KEY_SIZE_STATUS")
    .snapshot();
  equal(
    keySizeHistogram.values[0],
    0,
    "Actual and expected unchecked key size values should match"
  );
  equal(
    keySizeHistogram.values[1],
    16,
    "Actual and expected successful verifications of 2048-bit keys should match"
  );
  equal(
    keySizeHistogram.values[2] || 0,
    0,
    "Actual and expected successful verifications of 1024-bit keys should match"
  );
  equal(
    keySizeHistogram.values[3],
    68,
    "Actual and expected verification failures unrelated to key size should match"
  );

  run_next_test();
}

// Internally, specifying "port" -1 is the same as port 443. This tests that.
function run_port_equivalency_test(inPort, outPort) {
  Assert.ok(
    (inPort == 443 && outPort == -1) || (inPort == -1 && outPort == 443),
    "The two specified ports must be -1 and 443 (in any order)"
  );
  let certOverrideService = Cc[
    "@mozilla.org/security/certoverride;1"
  ].getService(Ci.nsICertOverrideService);
  let cert = constructCertFromFile("bad_certs/default-ee.pem");
  let expectedBits = Ci.nsICertOverrideService.ERROR_UNTRUSTED;
  let expectedTemporary = true;
  certOverrideService.rememberValidityOverride(
    "example.com",
    inPort,
    {},
    cert,
    expectedBits,
    expectedTemporary
  );
  let actualBits = {};
  let actualTemporary = {};
  Assert.ok(
    certOverrideService.hasMatchingOverride(
      "example.com",
      outPort,
      {},
      cert,
      actualBits,
      actualTemporary
    ),
    `override set on port ${inPort} should match port ${outPort}`
  );
  equal(
    actualBits.value,
    expectedBits,
    "input override bits should match output bits"
  );
  equal(
    actualTemporary.value,
    expectedTemporary,
    "input override temporary value should match output temporary value"
  );
  Assert.ok(
    !certOverrideService.hasMatchingOverride(
      "example.com",
      563,
      {},
      cert,
      {},
      {}
    ),
    `override set on port ${inPort} should not match port 563`
  );
  certOverrideService.clearValidityOverride("example.com", inPort, {});
  Assert.ok(
    !certOverrideService.hasMatchingOverride(
      "example.com",
      outPort,
      {},
      cert,
      actualBits,
      {}
    ),
    `override cleared on port ${inPort} should match port ${outPort}`
  );
  equal(actualBits.value, 0, "should have no bits set if there is no override");
}

function run_test() {
  run_port_equivalency_test(-1, 443);
  run_port_equivalency_test(443, -1);

  Services.prefs.setIntPref("security.OCSP.enabled", 1);
  add_tls_server_setup("BadCertAndPinningServer", "bad_certs");

  let fakeOCSPResponder = new HttpServer();
  fakeOCSPResponder.registerPrefixHandler("/", function(request, response) {
    response.setStatusLine(request.httpVersion, 500, "Internal Server Error");
  });
  fakeOCSPResponder.start(8888);

  add_simple_tests();
  add_localhost_tests();
  add_combo_tests();
  add_distrust_tests();

  add_test(function() {
    fakeOCSPResponder.stop(check_telemetry);
  });

  run_next_test();
}

function add_simple_tests() {
  add_cert_override_test(
    "expired.example.com",
    Ci.nsICertOverrideService.ERROR_TIME,
    SEC_ERROR_EXPIRED_CERTIFICATE
  );
  add_cert_override_test(
    "notyetvalid.example.com",
    Ci.nsICertOverrideService.ERROR_TIME,
    MOZILLA_PKIX_ERROR_NOT_YET_VALID_CERTIFICATE
  );
  add_cert_override_test(
    "before-epoch.example.com",
    Ci.nsICertOverrideService.ERROR_TIME,
    SEC_ERROR_INVALID_TIME
  );
  add_cert_override_test(
    "selfsigned.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT
  );
  add_cert_override_test(
    "unknownissuer.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    SEC_ERROR_UNKNOWN_ISSUER
  );
  add_cert_override_test(
    "expiredissuer.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE
  );
  add_cert_override_test(
    "notyetvalidissuer.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    MOZILLA_PKIX_ERROR_NOT_YET_VALID_ISSUER_CERTIFICATE
  );
  add_cert_override_test(
    "before-epoch-issuer.example.com",
    Ci.nsICertOverrideService.ERROR_TIME,
    SEC_ERROR_INVALID_TIME
  );
  add_cert_override_test(
    "md5signature.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED
  );
  add_cert_override_test(
    "emptyissuername.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    MOZILLA_PKIX_ERROR_EMPTY_ISSUER_NAME
  );
  // This has name information in the subject alternative names extension,
  // but not the subject common name.
  add_cert_override_test(
    "mismatch.example.com",
    Ci.nsICertOverrideService.ERROR_MISMATCH,
    SSL_ERROR_BAD_CERT_DOMAIN
  );
  // This has name information in the subject common name but not the subject
  // alternative names extension.
  add_cert_override_test(
    "mismatch-CN.example.com",
    Ci.nsICertOverrideService.ERROR_MISMATCH,
    SSL_ERROR_BAD_CERT_DOMAIN
  );

  // A Microsoft IIS utility generates self-signed certificates with
  // properties similar to the one this "host" will present.
  add_cert_override_test(
    "selfsigned-inadequateEKU.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT
  );

  add_prevented_cert_override_test(
    "inadequatekeyusage.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    SEC_ERROR_INADEQUATE_KEY_USAGE
  );

  // Test triggering the MitM detection. We don't set-up a proxy here. Just
  // set the pref. Without the pref set we expect an unkown issuer error.
  add_cert_override_test(
    "mitm.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    SEC_ERROR_UNKNOWN_ISSUER
  );
  add_test(function() {
    Services.prefs.setStringPref(
      "security.pki.mitm_canary_issuer",
      "CN=Test MITM Root"
    );
    let certOverrideService = Cc[
      "@mozilla.org/security/certoverride;1"
    ].getService(Ci.nsICertOverrideService);
    certOverrideService.clearValidityOverride("mitm.example.com", 8443, {});
    run_next_test();
  });
  add_cert_override_test(
    "mitm.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    MOZILLA_PKIX_ERROR_MITM_DETECTED
  );
  add_test(function() {
    Services.prefs.setStringPref(
      "security.pki.mitm_canary_issuer",
      "CN=Other MITM Root"
    );
    let certOverrideService = Cc[
      "@mozilla.org/security/certoverride;1"
    ].getService(Ci.nsICertOverrideService);
    certOverrideService.clearValidityOverride("mitm.example.com", 8443, {});
    run_next_test();
  });
  // If the canary issuer doesn't match the one we see, we exepct and unknown
  // issuer error.
  add_cert_override_test(
    "mitm.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    SEC_ERROR_UNKNOWN_ISSUER
  );
  // If security.pki.mitm_canary_issuer.enabled is false, there should always
  // be an unknown issuer error.
  add_test(function() {
    Services.prefs.setBoolPref(
      "security.pki.mitm_canary_issuer.enabled",
      false
    );
    let certOverrideService = Cc[
      "@mozilla.org/security/certoverride;1"
    ].getService(Ci.nsICertOverrideService);
    certOverrideService.clearValidityOverride("mitm.example.com", 8443, {});
    run_next_test();
  });
  add_cert_override_test(
    "mitm.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    SEC_ERROR_UNKNOWN_ISSUER
  );
  add_test(function() {
    Services.prefs.clearUserPref("security.pki.mitm_canary_issuer");
    run_next_test();
  });

  // This is intended to test the case where a verification has failed for one
  // overridable reason (e.g. unknown issuer) but then, in the process of
  // reporting that error, a non-overridable error is encountered. The
  // non-overridable error should be prioritized.
  add_test(function() {
    let rootCert = constructCertFromFile("bad_certs/test-ca.pem");
    setCertTrust(rootCert, ",,");
    run_next_test();
  });
  add_prevented_cert_override_test(
    "nsCertTypeCritical.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION
  );
  add_test(function() {
    let rootCert = constructCertFromFile("bad_certs/test-ca.pem");
    setCertTrust(rootCert, "CTu,,");
    run_next_test();
  });

  // Bug 990603: Apache documentation has recommended generating a self-signed
  // test certificate with basic constraints: CA:true. For compatibility, this
  // is a scenario in which an override is allowed.
  add_cert_override_test(
    "self-signed-end-entity-with-cA-true.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT
  );

  add_cert_override_test(
    "ca-used-as-end-entity.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY
  );

  // If an X.509 version 1 certificate is not a trust anchor, we will
  // encounter an overridable error.
  add_cert_override_test(
    "end-entity-issued-by-v1-cert.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA
  );
  // If we make that certificate a trust anchor, the connection will succeed.
  add_test(function() {
    let certOverrideService = Cc[
      "@mozilla.org/security/certoverride;1"
    ].getService(Ci.nsICertOverrideService);
    certOverrideService.clearValidityOverride(
      "end-entity-issued-by-v1-cert.example.com",
      8443,
      {}
    );
    let v1Cert = constructCertFromFile("bad_certs/v1Cert.pem");
    setCertTrust(v1Cert, "CTu,,");
    clearSessionCache();
    run_next_test();
  });
  add_connection_test(
    "end-entity-issued-by-v1-cert.example.com",
    PRErrorCodeSuccess
  );
  // Reset the trust for that certificate.
  add_test(function() {
    let v1Cert = constructCertFromFile("bad_certs/v1Cert.pem");
    setCertTrust(v1Cert, ",,");
    clearSessionCache();
    run_next_test();
  });

  // Due to compatibility issues, we allow overrides for certificates issued by
  // certificates that are not valid CAs.
  add_cert_override_test(
    "end-entity-issued-by-non-CA.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    SEC_ERROR_CA_CERT_INVALID
  );

  // This host presents a 1016-bit RSA key.
  add_cert_override_test(
    "inadequate-key-size-ee.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE
  );

  add_cert_override_test(
    "ipAddressAsDNSNameInSAN.example.com",
    Ci.nsICertOverrideService.ERROR_MISMATCH,
    SSL_ERROR_BAD_CERT_DOMAIN
  );
  add_cert_override_test(
    "noValidNames.example.com",
    Ci.nsICertOverrideService.ERROR_MISMATCH,
    SSL_ERROR_BAD_CERT_DOMAIN
  );
  add_cert_override_test(
    "badSubjectAltNames.example.com",
    Ci.nsICertOverrideService.ERROR_MISMATCH,
    SSL_ERROR_BAD_CERT_DOMAIN
  );

  add_cert_override_test(
    "bug413909.xn--hxajbheg2az3al.xn--jxalpdlp",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    SEC_ERROR_UNKNOWN_ISSUER
  );
  add_test(function() {
    // At this point, the override for bug413909.xn--hxajbheg2az3al.xn--jxalpdlp
    // is still valid. Do some additional tests relating to IDN handling.
    let certOverrideService = Cc[
      "@mozilla.org/security/certoverride;1"
    ].getService(Ci.nsICertOverrideService);
    let uri = Services.io.newURI(
      "https://bug413909.xn--hxajbheg2az3al.xn--jxalpdlp"
    );
    let cert = constructCertFromFile("bad_certs/idn-certificate.pem");
    Assert.ok(
      certOverrideService.hasMatchingOverride(
        uri.asciiHost,
        8443,
        {},
        cert,
        {},
        {}
      ),
      "IDN certificate should have matching override using ascii host"
    );
    Assert.throws(
      () =>
        !certOverrideService.hasMatchingOverride(
          uri.displayHost,
          8443,
          {},
          cert,
          {},
          {}
        ),
      /NS_ERROR_ILLEGAL_VALUE/,
      "IDN certificate should not have matching override using (non-ascii) host"
    );
    let invalidHost = uri.asciiHost.replace(/./g, c =>
      String.fromCharCode(c.charCodeAt(0) | 0x100)
    );
    Assert.throws(
      () =>
        !certOverrideService.hasMatchingOverride(
          invalidHost,
          8443,
          {},
          cert,
          {},
          {}
        ),
      /NS_ERROR_ILLEGAL_VALUE/,
      "hasMatchingOverride should not truncate high-bytes"
    );
    run_next_test();
  });

  add_test(function() {
    // Add a bunch of overrides...
    let certOverrideService = Cc[
      "@mozilla.org/security/certoverride;1"
    ].getService(Ci.nsICertOverrideService);
    let cert = constructCertFromFile("bad_certs/default-ee.pem");
    let expectedBits = Ci.nsICertOverrideService.ERROR_UNTRUSTED;
    certOverrideService.rememberValidityOverride(
      "example.com",
      443,
      {},
      cert,
      expectedBits,
      false
    );
    Assert.ok(
      certOverrideService.hasMatchingOverride(
        "example.com",
        443,
        {},
        cert,
        {},
        {}
      ),
      "Should have added override for example.com:443"
    );
    certOverrideService.rememberValidityOverride(
      "example.com",
      80,
      {},
      cert,
      expectedBits,
      false
    );
    certOverrideService.rememberValidityOverride(
      "::1",
      80,
      {},
      cert,
      expectedBits,
      false
    );
    Assert.ok(
      certOverrideService.hasMatchingOverride(
        "example.com",
        80,
        {},
        cert,
        {},
        {}
      ),
      "Should have added override for example.com:80"
    );
    certOverrideService.rememberValidityOverride(
      "example.org",
      443,
      {},
      cert,
      expectedBits,
      false
    );
    Assert.ok(
      certOverrideService.hasMatchingOverride(
        "example.org",
        443,
        {},
        cert,
        {},
        {}
      ),
      "Should have added override for example.org:443"
    );
    Assert.ok(
      certOverrideService.hasMatchingOverride("::1", 80, {}, cert, {}, {}),
      "Should have added override for [::1]:80"
    );
    // When in a private browsing context, overrides added in non-private
    // contexts should match (but not vice-versa).
    Assert.ok(
      certOverrideService.hasMatchingOverride(
        "example.org",
        443,
        { privateBrowsingId: 1 },
        cert,
        {},
        {}
      ),
      "Should have override for example.org:443 with privateBrowsingId 1"
    );
    Assert.ok(
      certOverrideService.hasMatchingOverride(
        "example.org",
        443,
        { privateBrowsingId: 2 },
        cert,
        {},
        {}
      ),
      "Should have override for example.org:443 with privateBrowsingId 2"
    );
    Assert.ok(
      certOverrideService.hasMatchingOverride(
        "example.org",
        443,
        { firstPartyDomain: "example.org", userContextId: 1 },
        cert,
        {},
        {}
      ),
      "Should ignore firstPartyDomain and userContextId when checking overrides"
    );
    certOverrideService.rememberValidityOverride(
      "example.org",
      80,
      {},
      cert,
      expectedBits,
      true
    );
    Assert.ok(
      certOverrideService.hasMatchingOverride(
        "example.org",
        80,
        {},
        cert,
        {},
        {}
      ),
      "Should have added override for example.org:80"
    );
    certOverrideService.rememberValidityOverride(
      "test.example.org",
      443,
      { firstPartyDomain: "example.org", userContextId: 1 },
      cert,
      expectedBits,
      false
    );
    Assert.ok(
      certOverrideService.hasMatchingOverride(
        "test.example.org",
        443,
        {},
        cert,
        {},
        {}
      ),
      "Should ignore firstPartyDomain and userContextId when adding overrides"
    );
    Assert.ok(
      certOverrideService.hasMatchingOverride(
        "test.example.org",
        443,
        { firstPartyDomain: "example.com", userContextId: 2 },
        cert,
        {},
        {}
      ),
      "Should ignore firstPartyDomain and userContextId when checking overrides"
    );
    certOverrideService.rememberValidityOverride(
      "example.test",
      443,
      { privateBrowsingId: 1 },
      cert,
      expectedBits,
      false
    );
    Assert.ok(
      certOverrideService.hasMatchingOverride(
        "example.test",
        443,
        { privateBrowsingId: 1 },
        cert,
        {},
        {}
      ),
      "Should have added override for example.test:443 with privateBrowsingId 1"
    );
    Assert.ok(
      !certOverrideService.hasMatchingOverride(
        "example.test",
        443,
        { privateBrowsingId: 2 },
        cert,
        {},
        {}
      ),
      "Should not have override for example.test:443 with privateBrowsingId 2"
    );
    Assert.ok(
      !certOverrideService.hasMatchingOverride(
        "example.test",
        443,
        {},
        cert,
        {},
        {}
      ),
      "Should not have override for example.test:443 with non-private OriginAttributes"
    );
    // Clear them all...
    certOverrideService.clearAllOverrides();

    // And ensure they're all gone.
    Assert.ok(
      !certOverrideService.hasMatchingOverride(
        "example.com",
        443,
        {},
        cert,
        {},
        {}
      ),
      "Should have removed override for example.com:443"
    );
    Assert.ok(
      !certOverrideService.hasMatchingOverride(
        "example.com",
        80,
        {},
        cert,
        {},
        {}
      ),
      "Should have removed override for example.com:80"
    );
    Assert.ok(
      !certOverrideService.hasMatchingOverride(
        "example.org",
        443,
        {},
        cert,
        {},
        {}
      ),
      "Should have removed override for example.org:443"
    );
    Assert.ok(
      !certOverrideService.hasMatchingOverride(
        "example.org",
        80,
        {},
        cert,
        {},
        {}
      ),
      "Should have removed override for example.org:80"
    );
    Assert.ok(
      !certOverrideService.hasMatchingOverride(
        "example.org",
        443,
        { privateBrowsingId: 1 },
        cert,
        {},
        {}
      ),
      "Should have removed override for example.org:443 with privateBrowsingId 1"
    );

    run_next_test();
  });
}

function add_localhost_tests() {
  add_cert_override_test(
    "localhost",
    Ci.nsICertOverrideService.ERROR_MISMATCH |
      Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    SEC_ERROR_UNKNOWN_ISSUER
  );
  add_cert_override_test(
    "127.0.0.1",
    Ci.nsICertOverrideService.ERROR_MISMATCH,
    SSL_ERROR_BAD_CERT_DOMAIN
  );
  add_cert_override_test(
    "::1",
    Ci.nsICertOverrideService.ERROR_MISMATCH,
    SSL_ERROR_BAD_CERT_DOMAIN
  );
}

function add_combo_tests() {
  add_cert_override_test(
    "mismatch-expired.example.com",
    Ci.nsICertOverrideService.ERROR_MISMATCH |
      Ci.nsICertOverrideService.ERROR_TIME,
    SSL_ERROR_BAD_CERT_DOMAIN
  );
  add_cert_override_test(
    "mismatch-notYetValid.example.com",
    Ci.nsICertOverrideService.ERROR_MISMATCH |
      Ci.nsICertOverrideService.ERROR_TIME,
    SSL_ERROR_BAD_CERT_DOMAIN
  );
  add_cert_override_test(
    "mismatch-untrusted.example.com",
    Ci.nsICertOverrideService.ERROR_MISMATCH |
      Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    SEC_ERROR_UNKNOWN_ISSUER
  );
  add_cert_override_test(
    "untrusted-expired.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED |
      Ci.nsICertOverrideService.ERROR_TIME,
    SEC_ERROR_UNKNOWN_ISSUER
  );
  add_cert_override_test(
    "mismatch-untrusted-expired.example.com",
    Ci.nsICertOverrideService.ERROR_MISMATCH |
      Ci.nsICertOverrideService.ERROR_UNTRUSTED |
      Ci.nsICertOverrideService.ERROR_TIME,
    SEC_ERROR_UNKNOWN_ISSUER
  );

  add_cert_override_test(
    "md5signature-expired.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED |
      Ci.nsICertOverrideService.ERROR_TIME,
    SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED
  );

  add_cert_override_test(
    "ca-used-as-end-entity-name-mismatch.example.com",
    Ci.nsICertOverrideService.ERROR_MISMATCH |
      Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY
  );
}

function add_distrust_tests() {
  // Before we specifically distrust this certificate, it should be trusted.
  add_connection_test("untrusted.example.com", PRErrorCodeSuccess);

  add_distrust_test(
    "bad_certs/default-ee.pem",
    "untrusted.example.com",
    SEC_ERROR_UNTRUSTED_CERT
  );

  add_distrust_test(
    "bad_certs/other-test-ca.pem",
    "untrustedissuer.example.com",
    SEC_ERROR_UNTRUSTED_ISSUER
  );

  add_distrust_test(
    "bad_certs/test-ca.pem",
    "ca-used-as-end-entity.example.com",
    SEC_ERROR_UNTRUSTED_ISSUER
  );
}

function add_distrust_test(certFileName, hostName, expectedResult) {
  let certToDistrust = constructCertFromFile(certFileName);

  add_test(function() {
    // Add an entry to the NSS certDB that says to distrust the cert
    setCertTrust(certToDistrust, "pu,,");
    clearSessionCache();
    run_next_test();
  });
  add_prevented_cert_override_test(
    hostName,
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    expectedResult
  );
  add_test(function() {
    setCertTrust(certToDistrust, "u,,");
    run_next_test();
  });
}
