/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

do_get_profile();

add_tls_server_setup(
  "DelegatedCredentialsServer",
  "test_delegated_credentials_weak"
);

// Test:
// Server certificate supports DC
// Server DC support enabled, but presents a weak (RSA1016) key in the DC
// Client DC support enabled
// Result: Inadequate key size error
add_test(function() {
  clearSessionCache();
  Services.prefs.setBoolPref("security.tls.enable_delegated_credentials", true);
  run_next_test();
});
add_connection_test(
  "delegated-weak.example.com",
  MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE,
  null,
  // We'll never |mHaveCipherSuiteAndProtocol|,
  // and therefore can't check IsDelegatedCredential
  null
);
