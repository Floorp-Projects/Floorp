/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Tests handling of certificates marked as permitting delegated credentials

function shouldBeDelegatedCredential(aTransportSecurityInfo) {
  Assert.ok(
    aTransportSecurityInfo.isDelegatedCredential,
    "This host should have used a delegated credential"
  );
}

function shouldNotBeDelegatedCredential(aTransportSecurityInfo) {
  Assert.ok(
    !aTransportSecurityInfo.isDelegatedCredential,
    "This host should not have used a delegated credential"
  );
}

do_get_profile();

add_tls_server_setup(
  "DelegatedCredentialsServer",
  "test_delegated_credentials"
);

// Test:
// Server certificate supports DC
// Server DC support enabled
// Client DC support disabled
// Result: Successful connection without DC
add_test(function() {
  clearSessionCache();
  Services.prefs.setBoolPref(
    "security.tls.enable_delegated_credentials",
    false
  );
  run_next_test();
});
add_connection_test(
  "delegated-enabled.example.com",
  PRErrorCodeSuccess,
  null,
  shouldNotBeDelegatedCredential
);

// Test:
// Server certificate does not support DC
// Server DC support enabled
// Client DC support enabled
// Result: SSL_ERROR_DC_INVALID_KEY_USAGE from client when
//         checking DC against EE cert, no DC in aTransportSecurityInfo.
add_test(function() {
  clearSessionCache();
  Services.prefs.setBoolPref("security.tls.enable_delegated_credentials", true);
  run_next_test();
});
add_connection_test(
  "standard-enabled.example.com",
  SSL_ERROR_DC_INVALID_KEY_USAGE,
  null,
  // We'll never |mHaveCipherSuiteAndProtocol|,
  // and therefore can't check IsDelegatedCredential
  function() {}
);

// Test:
// Server certificate supports DC
// Server DC support disabled
// Client DC support enabled
// Result: Successful connection without DC
add_connection_test(
  "delegated-disabled.example.com",
  PRErrorCodeSuccess,
  null,
  shouldNotBeDelegatedCredential
);

// Test:
// Server certificate supports DC
// Server DC support enabled
// Client DC support enabled
// Result: Successful connection with DC
add_connection_test(
  "delegated-enabled.example.com",
  PRErrorCodeSuccess,
  null,
  shouldBeDelegatedCredential
);
