/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

do_get_profile();

const certDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

add_tls_server_setup(
  "SanctionsTestServer",
  "test_sanctions",
  /* Don't try to load non-existent test-ca.pem */ false
);

addCertFromFile(certDB, "test_sanctions/symantec-test-ca.pem", "CTu,u,u");

// Add the necessary intermediates. This is important because the test server,
// though it will attempt to send along an intermediate, isn't able to reliably
// pick between the intermediate-other-crossigned and intermediate-other.
add_test(function() {
  addCertFromFile(
    certDB,
    "test_sanctions/symantec-intermediate-allowlisted.pem",
    ",,"
  );
  addCertFromFile(
    certDB,
    "test_sanctions/symantec-intermediate-other.pem",
    ",,"
  );
  run_next_test();
});

add_connection_test(
  "symantec-not-allowlisted-before-cutoff.example.com",
  MOZILLA_PKIX_ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED,
  null,
  null
);

add_connection_test(
  "symantec-not-allowlisted-after-cutoff.example.com",
  MOZILLA_PKIX_ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED,
  null,
  null
);

// Add a cross-signed intermediate into the database, and ensure we still get
// the expected error.
add_test(function() {
  addCertFromFile(
    certDB,
    "test_sanctions/symantec-intermediate-other-crossigned.pem",
    ",,"
  );
  run_next_test();
});

add_connection_test(
  "symantec-not-allowlisted-before-cutoff.example.com",
  MOZILLA_PKIX_ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED,
  null,
  null
);

// Load the Apple EE cert and its intermediate, then verify
// it at a reasonable time and make sure the allowlists work
add_task(async function() {
  addCertFromFile(
    certDB,
    "test_sanctions/apple-ist-ca-8-g1-intermediate.pem",
    ",,"
  );
  let allowlistedCert = constructCertFromFile(
    "test_sanctions/cds-apple-com.pem"
  );

  // Since we don't want to actually try to fetch OCSP for this certificate,
  // (as an external fetch is bad in the tests), disable OCSP first.
  Services.prefs.setIntPref("security.OCSP.enabled", 0);

  // (new Date("2020-01-01")).getTime() / 1000
  const VALIDATION_TIME = 1577836800;

  await checkCertErrorGenericAtTime(
    certDB,
    allowlistedCert,
    PRErrorCodeSuccess,
    certificateUsageSSLServer,
    VALIDATION_TIME
  );
});
