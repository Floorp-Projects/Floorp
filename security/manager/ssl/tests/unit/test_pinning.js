// -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
// - FAIL: exclude-subdomains.pinning.example.com services an incorrect cert
// (TODO: test using verifyCertnow)
// - PASS: sub.exclude-subdomains.pinning.example.com serves an incorrect cert

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);

function test_strict() {
  // In strict mode, we always evaluate pinning data, regardless of whether the
  // issuer is a built-in trust anchor.
  add_test(function() {
    Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 2);
    run_next_test();
  });

  // Issued by otherCA, which is not in the pinset for pinning.example.com.
  add_connection_test("bad.include-subdomains.pinning.example.com",
    getXPCOMStatusFromNSS(SEC_ERROR_APPLICATION_CALLBACK_ERROR));

  // These domains serve certs that match the pinset.
  add_connection_test("include-subdomains.pinning.example.com", Cr.NS_OK);
  add_connection_test("good.include-subdomains.pinning.example.com", Cr.NS_OK);
  add_connection_test("exclude-subdomains.pinning.example.com", Cr.NS_OK);

  // This domain serves a cert that doesn't match the pinset, but subdomains
  // are excluded.
  add_connection_test("sub.exclude-subdomains.pinning.example.com", Cr.NS_OK);

  // This domain's pinset is exactly the same as
  // include-subdomains.pinning.example.com, serves the same cert as
  // bad.include-subdomains.pinning.example.com, but it should pass because
  // it's in test_mode.
  add_connection_test("test-mode.pinning.example.com", Cr.NS_OK);
}

function test_mitm() {
  // In MITM mode, we allow pinning to pass if the chain resolves to any
  // user-specified trust anchor, even if it is not in the pinset.
  add_test(function() {
    Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 1);
    run_next_test();
  });

  add_connection_test("include-subdomains.pinning.example.com", Cr.NS_OK);
  add_connection_test("good.include-subdomains.pinning.example.com", Cr.NS_OK);

  // In this case, even though otherCA is not in the pinset, it is a
  // user-specified trust anchor and the pinning check succeeds.
  add_connection_test("bad.include-subdomains.pinning.example.com", Cr.NS_OK);

  add_connection_test("exclude-subdomains.pinning.example.com", Cr.NS_OK);
  add_connection_test("sub.exclude-subdomains.pinning.example.com", Cr.NS_OK);
  add_connection_test("test-mode.pinning.example.com", Cr.NS_OK);
};

function test_disabled() {
  // Disable pinning.
  add_test(function() {
    Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 0);
    run_next_test();
  });

  add_connection_test("include-subdomains.pinning.example.com", Cr.NS_OK);
  add_connection_test("good.include-subdomains.pinning.example.com", Cr.NS_OK);
  add_connection_test("bad.include-subdomains.pinning.example.com", Cr.NS_OK);
  add_connection_test("exclude-subdomains.pinning.example.com", Cr.NS_OK);
  add_connection_test("sub.exclude-subdomains.pinning.example.com", Cr.NS_OK);
  add_connection_test("test-mode.pinning.example.com", Cr.NS_OK);
}

function check_pinning_telemetry() {
  let service = Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);
  let prod_histogram = service.getHistogramById("CERT_PINNING_RESULTS")
                         .snapshot();
  let test_histogram = service.getHistogramById("CERT_PINNING_TEST_RESULTS")
                         .snapshot();
  // Because all of our test domains are pinned to user-specified trust
  // anchors, effectively only strict mode gets evaluated
  do_check_eq(prod_histogram.counts[0], 1); // Failure count
  do_check_eq(prod_histogram.counts[1], 3); // Success count
  do_check_eq(test_histogram.counts[0], 1); // Failure count
  do_check_eq(test_histogram.counts[1], 0); // Success count

  let moz_prod_histogram = service.getHistogramById("CERT_PINNING_MOZ_RESULTS")
                             .snapshot();
  let moz_test_histogram =
    service.getHistogramById("CERT_PINNING_MOZ_TEST_RESULTS").snapshot();
  do_check_eq(moz_prod_histogram.counts[0], 0); // Failure count
  do_check_eq(moz_prod_histogram.counts[1], 0); // Success count
  do_check_eq(moz_test_histogram.counts[0], 0); // Failure count
  do_check_eq(moz_test_histogram.counts[1], 0); // Success count

  run_next_test();
}

function run_test() {
  add_tls_server_setup("BadCertServer");

  // Add a user-specified trust anchor.
  addCertFromFile(certdb, "tlsserver/other-test-ca.der", "CTu,u,u");

  test_strict();
  test_mitm();
  test_disabled();

  add_test(function () {
    check_pinning_telemetry();
  });
  run_next_test();
}
