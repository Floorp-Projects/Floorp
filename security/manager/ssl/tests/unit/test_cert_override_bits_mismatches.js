// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// Test that when an override exists for a (host, certificate) pair,
// connections will succeed only if the set of error bits in the override
// contains each bit in the set of encountered error bits.
// Strangely, it is possible to store an override with an empty set of error
// bits, so this tests that too.

do_get_profile();

function add_override_bits_mismatch_test(host, certPath, expectedBits,
                                         expectedError) {
  const MAX_BITS = Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                   Ci.nsICertOverrideService.ERROR_MISMATCH |
                   Ci.nsICertOverrideService.ERROR_TIME;
  let cert = constructCertFromFile(certPath);
  let certOverrideService = Cc["@mozilla.org/security/certoverride;1"]
                              .getService(Ci.nsICertOverrideService);
  for (let overrideBits = 0; overrideBits <= MAX_BITS; overrideBits++) {
    add_test(function() {
      certOverrideService.clearValidityOverride(host, 8443);
      certOverrideService.rememberValidityOverride(host, 8443, cert,
                                                   overrideBits, true);
      run_next_test();
    });
    // The override will succeed only if the set of error bits in the override
    // contains each bit in the set of expected error bits.
    let successExpected = (overrideBits | expectedBits) == overrideBits;
    add_connection_test(host, successExpected ? PRErrorCodeSuccess
                                              : expectedError);
  }
}

function run_test() {
  Services.prefs.setIntPref("security.OCSP.enabled", 1);
  add_tls_server_setup("BadCertServer", "bad_certs");

  let fakeOCSPResponder = new HttpServer();
  fakeOCSPResponder.registerPrefixHandler("/", function (request, response) {
    response.setStatusLine(request.httpVersion, 500, "Internal Server Error");
  });
  fakeOCSPResponder.start(8888);

  add_override_bits_mismatch_test("unknownissuer.example.com",
                                  "bad_certs/unknownissuer.pem",
                                  Ci.nsICertOverrideService.ERROR_UNTRUSTED,
                                  SEC_ERROR_UNKNOWN_ISSUER);
  add_override_bits_mismatch_test("mismatch.example.com",
                                  "bad_certs/mismatch.pem",
                                  Ci.nsICertOverrideService.ERROR_MISMATCH,
                                  SSL_ERROR_BAD_CERT_DOMAIN);
  add_override_bits_mismatch_test("expired.example.com",
                                  "bad_certs/expired-ee.pem",
                                  Ci.nsICertOverrideService.ERROR_TIME,
                                  SEC_ERROR_EXPIRED_CERTIFICATE);

  add_override_bits_mismatch_test("mismatch-untrusted.example.com",
                                  "bad_certs/mismatch-untrusted.pem",
                                  Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                                  Ci.nsICertOverrideService.ERROR_MISMATCH,
                                  SEC_ERROR_UNKNOWN_ISSUER);
  add_override_bits_mismatch_test("untrusted-expired.example.com",
                                  "bad_certs/untrusted-expired.pem",
                                  Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                                  Ci.nsICertOverrideService.ERROR_TIME,
                                  SEC_ERROR_UNKNOWN_ISSUER);
  add_override_bits_mismatch_test("mismatch-expired.example.com",
                                  "bad_certs/mismatch-expired.pem",
                                  Ci.nsICertOverrideService.ERROR_MISMATCH |
                                  Ci.nsICertOverrideService.ERROR_TIME,
                                  SSL_ERROR_BAD_CERT_DOMAIN);

  add_override_bits_mismatch_test("mismatch-untrusted-expired.example.com",
                                  "bad_certs/mismatch-untrusted-expired.pem",
                                  Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                                  Ci.nsICertOverrideService.ERROR_MISMATCH |
                                  Ci.nsICertOverrideService.ERROR_TIME,
                                  SEC_ERROR_UNKNOWN_ISSUER);

  add_test(function () {
    fakeOCSPResponder.stop(run_next_test);
  });

  run_next_test();
}
