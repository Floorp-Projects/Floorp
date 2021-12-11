/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Public Name = delegated-enabled.example.com
const ECH_CONFIG_FIXED =
  "AFL+CgBOTQAgACCKB1Y5SfrGIyk27W82xPpzWTDs3q72c04xSurDWlb9CgAEAAEAAwBkAB1kZWxlZ2F0ZWQtZW5hYmxlZC5leGFtcGxlLmNvbQAA";

do_get_profile();

// An arbitrary, non-ECH server.
add_tls_server_setup(
  "DelegatedCredentialsServer",
  "test_delegated_credentials"
);

add_test(function() {
  clearSessionCache();
  run_next_test();
});

// Connect, sending ECH. The server is not configured for it,
// but *is* authoritative for the public name.
add_connection_test(
  "delegated-disabled.example.com",
  SSL_ERROR_ECH_RETRY_WITHOUT_ECH,
  null,
  null,
  null,
  null,
  ECH_CONFIG_FIXED
);
