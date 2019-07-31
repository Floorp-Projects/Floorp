/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Tests handling of certificates that are selected to emit a distrust warning
// to the console.

function shouldBeImminentlyDistrusted(aTransportSecurityInfo) {
  let isDistrust =
    aTransportSecurityInfo.securityState &
    Ci.nsIWebProgressListener.STATE_CERT_DISTRUST_IMMINENT;
  Assert.ok(isDistrust, "This host should be imminently distrusted");
}

function shouldNotBeImminentlyDistrusted(aTransportSecurityInfo) {
  let isDistrust =
    aTransportSecurityInfo.securityState &
    Ci.nsIWebProgressListener.STATE_CERT_DISTRUST_IMMINENT;
  Assert.ok(!isDistrust, "This host should not be imminently distrusted");
}

do_get_profile();

add_tls_server_setup("BadCertAndPinningServer", "bad_certs");

add_connection_test(
  "imminently-distrusted.example.com",
  PRErrorCodeSuccess,
  null,
  shouldBeImminentlyDistrusted
);

add_connection_test(
  "include-subdomains.pinning.example.com",
  PRErrorCodeSuccess,
  null,
  shouldNotBeImminentlyDistrusted
);
