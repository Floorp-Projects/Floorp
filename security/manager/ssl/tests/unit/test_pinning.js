// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// For all cases, the acceptable pinset includes only certificates pinned to
// Test End Entity Cert (signed by issuer testCA). Other certificates
// are issued by otherCA, which is never in the pinset but is a user-specified
// trust anchor. This test covers multiple cases:
//
// Pinned domain include-subdomains.pinning.example.com includes subdomains
// - PASS: include-subdomains.pinning.example.com serves a correct cert
// - PASS: good.include-subdomains.pinning.example.com serves a correct cert
// - FAIL (strict): bad.include-subdomains.pinning.example.com serves a cert
// not in the pinset
// - PASS (mitm): bad.include-subdomains.pinning.example.com serves a cert not
// in the pinset, but issued by a user-specified trust domain
//
// Pinned domain exclude-subdomains.pinning.example.com excludes subdomains
// - PASS: exclude-subdomains.pinning.example.com serves a correct cert
// - FAIL: exclude-subdomains.pinning.example.com serves an incorrect cert
// (TODO: test using verifyCertNow)
// - PASS: sub.exclude-subdomains.pinning.example.com serves an incorrect cert

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);

function add_clear_override(host) {
  add_test(function() {
    let certOverrideService = Cc["@mozilla.org/security/certoverride;1"]
                                .getService(Ci.nsICertOverrideService);
    certOverrideService.clearValidityOverride(host, 8443);
    run_next_test();
  });
}

function test_strict() {
  // In strict mode, we always evaluate pinning data, regardless of whether the
  // issuer is a built-in trust anchor. We only enforce pins that are not in
  // test mode.
  add_test(function() {
    Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 2);
    run_next_test();
  });

  // Normally this is overridable. But, since we have pinning information for
  // this host, we don't allow overrides.
  add_prevented_cert_override_test(
    "unknownissuer.include-subdomains.pinning.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    SEC_ERROR_UNKNOWN_ISSUER);
  add_clear_override("unknownissuer.include-subdomains.pinning.example.com");

  // Issued by otherCA, which is not in the pinset for pinning.example.com.
  add_connection_test("bad.include-subdomains.pinning.example.com",
                      MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE);

  // Check that using a FQDN doesn't bypass pinning.
  add_connection_test("bad.include-subdomains.pinning.example.com.",
                      MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE);
  // For some reason this is also navigable (see bug 1118522).
  add_connection_test("bad.include-subdomains.pinning.example.com..",
                      MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE);

  // These domains serve certs that match the pinset.
  add_connection_test("include-subdomains.pinning.example.com",
                      PRErrorCodeSuccess);
  add_connection_test("good.include-subdomains.pinning.example.com",
                      PRErrorCodeSuccess);
  add_connection_test("exclude-subdomains.pinning.example.com",
                      PRErrorCodeSuccess);

  // This domain serves a cert that doesn't match the pinset, but subdomains
  // are excluded.
  add_connection_test("sub.exclude-subdomains.pinning.example.com",
                      PRErrorCodeSuccess);

  // This domain's pinset is exactly the same as
  // include-subdomains.pinning.example.com, serves the same cert as
  // bad.include-subdomains.pinning.example.com, but it should pass because
  // it's in test_mode.
  add_connection_test("test-mode.pinning.example.com",
                      PRErrorCodeSuccess);
  // Similarly, this pin is in test-mode, so it should be overridable.
  add_cert_override_test("unknownissuer.test-mode.pinning.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         SEC_ERROR_UNKNOWN_ISSUER);
  add_clear_override("unknownissuer.test-mode.pinning.example.com");
}

function test_mitm() {
  // In MITM mode, we allow pinning to pass if the chain resolves to any
  // user-specified trust anchor, even if it is not in the pinset.
  add_test(function() {
    Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 1);
    run_next_test();
  });

  add_connection_test("include-subdomains.pinning.example.com",
                      PRErrorCodeSuccess);
  add_connection_test("good.include-subdomains.pinning.example.com",
                      PRErrorCodeSuccess);

  // Normally this is overridable. But, since we have pinning information for
  // this host, we don't allow overrides (since building a trusted chain fails,
  // we have no reason to believe this was issued by a user-added trust
  // anchor, so we can't allow overrides for it).
  add_prevented_cert_override_test(
    "unknownissuer.include-subdomains.pinning.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    SEC_ERROR_UNKNOWN_ISSUER);
  add_clear_override("unknownissuer.include-subdomains.pinning.example.com");

  // In this case, even though otherCA is not in the pinset, it is a
  // user-specified trust anchor and the pinning check succeeds.
  add_connection_test("bad.include-subdomains.pinning.example.com",
                      PRErrorCodeSuccess);

  add_connection_test("exclude-subdomains.pinning.example.com",
                      PRErrorCodeSuccess);
  add_connection_test("sub.exclude-subdomains.pinning.example.com",
                      PRErrorCodeSuccess);
  add_connection_test("test-mode.pinning.example.com", PRErrorCodeSuccess);
  add_cert_override_test("unknownissuer.test-mode.pinning.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         SEC_ERROR_UNKNOWN_ISSUER);
  add_clear_override("unknownissuer.test-mode.pinning.example.com");
};

function test_disabled() {
  // Disable pinning.
  add_test(function() {
    Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 0);
    run_next_test();
  });

  add_connection_test("include-subdomains.pinning.example.com",
                      PRErrorCodeSuccess);
  add_connection_test("good.include-subdomains.pinning.example.com",
                      PRErrorCodeSuccess);
  add_connection_test("bad.include-subdomains.pinning.example.com",
                      PRErrorCodeSuccess);
  add_connection_test("exclude-subdomains.pinning.example.com",
                      PRErrorCodeSuccess);
  add_connection_test("sub.exclude-subdomains.pinning.example.com",
                      PRErrorCodeSuccess);
  add_connection_test("test-mode.pinning.example.com", PRErrorCodeSuccess);

  add_cert_override_test("unknownissuer.include-subdomains.pinning.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         SEC_ERROR_UNKNOWN_ISSUER);
  add_clear_override("unknownissuer.include-subdomains.pinning.example.com");
  add_cert_override_test("unknownissuer.test-mode.pinning.example.com",
                         Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                         SEC_ERROR_UNKNOWN_ISSUER);
  add_clear_override("unknownissuer.test-mode.pinning.example.com");
}

function test_enforce_test_mode() {
  // In enforce test mode, we always enforce all pins, even test pins.
  add_test(function() {
    Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 3);
    run_next_test();
  });

  // Normally this is overridable. But, since we have pinning information for
  // this host, we don't allow overrides.
  add_prevented_cert_override_test(
    "unknownissuer.include-subdomains.pinning.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    SEC_ERROR_UNKNOWN_ISSUER);
  add_clear_override("unknownissuer.include-subdomains.pinning.example.com");

  // Issued by otherCA, which is not in the pinset for pinning.example.com.
  add_connection_test("bad.include-subdomains.pinning.example.com",
                      MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE);

  // These domains serve certs that match the pinset.
  add_connection_test("include-subdomains.pinning.example.com",
                      PRErrorCodeSuccess);
  add_connection_test("good.include-subdomains.pinning.example.com",
                      PRErrorCodeSuccess);
  add_connection_test("exclude-subdomains.pinning.example.com",
                      PRErrorCodeSuccess);

  // This domain serves a cert that doesn't match the pinset, but subdomains
  // are excluded.
  add_connection_test("sub.exclude-subdomains.pinning.example.com",
                      PRErrorCodeSuccess);

  // This domain's pinset is exactly the same as
  // include-subdomains.pinning.example.com, serves the same cert as
  // bad.include-subdomains.pinning.example.com, is in test-mode, but we are
  // enforcing test mode pins.
  add_connection_test("test-mode.pinning.example.com",
                      MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE);
  // Normally this is overridable. But, since we have pinning information for
  // this host (and since we're enforcing test mode), we don't allow overrides.
  add_prevented_cert_override_test(
    "unknownissuer.test-mode.pinning.example.com",
    Ci.nsICertOverrideService.ERROR_UNTRUSTED,
    SEC_ERROR_UNKNOWN_ISSUER);
  add_clear_override("unknownissuer.test-mode.pinning.example.com");
}

function check_pinning_telemetry() {
  let service = Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);
  let prod_histogram = service.getHistogramById("CERT_PINNING_RESULTS")
                         .snapshot();
  let test_histogram = service.getHistogramById("CERT_PINNING_TEST_RESULTS")
                         .snapshot();
  // Because all of our test domains are pinned to user-specified trust
  // anchors, effectively only strict mode and enforce test-mode get evaluated
  equal(prod_histogram.counts[0], 4,
        "Actual and expected prod (non-Mozilla) failure count should match");
  equal(prod_histogram.counts[1], 4,
        "Actual and expected prod (non-Mozilla) success count should match");
  equal(test_histogram.counts[0], 2,
        "Actual and expected test (non-Mozilla) failure count should match");
  equal(test_histogram.counts[1], 0,
        "Actual and expected test (non-Mozilla) success count should match");

  let moz_prod_histogram = service.getHistogramById("CERT_PINNING_MOZ_RESULTS")
                             .snapshot();
  let moz_test_histogram =
    service.getHistogramById("CERT_PINNING_MOZ_TEST_RESULTS").snapshot();
  equal(moz_prod_histogram.counts[0], 0,
        "Actual and expected prod (Mozilla) failure count should match");
  equal(moz_prod_histogram.counts[1], 0,
        "Actual and expected prod (Mozilla) success count should match");
  equal(moz_test_histogram.counts[0], 0,
        "Actual and expected test (Mozilla) failure count should match");
  equal(moz_test_histogram.counts[1], 0,
        "Actual and expected test (Mozilla) success count should match");

  let per_host_histogram =
    service.getHistogramById("CERT_PINNING_MOZ_RESULTS_BY_HOST").snapshot();
  equal(per_host_histogram.counts[0], 0,
        "Actual and expected per host (Mozilla) failure count should match");
  equal(per_host_histogram.counts[1], 2,
        "Actual and expected per host (Mozilla) success count should match");
  run_next_test();
}

function run_test() {
  add_tls_server_setup("BadCertServer");

  // Add a user-specified trust anchor.
  addCertFromFile(certdb, "tlsserver/other-test-ca.pem", "CTu,u,u");

  test_strict();
  test_mitm();
  test_disabled();
  test_enforce_test_mode();

  add_test(function () {
    check_pinning_telemetry();
  });
  run_next_test();
}
