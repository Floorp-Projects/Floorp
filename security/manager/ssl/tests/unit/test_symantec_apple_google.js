/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Tests handling of certificates issued by Symantec. If such
// certificates have a notBefore before 1 June 2016, and are not
// issued by an Apple or Google intermediate, they should emit a
// warning to the console.

function shouldBeImminentlyDistrusted(aTransportSecurityInfo) {
  let isDistrust = aTransportSecurityInfo.securityState &
                     Ci.nsIWebProgressListener.STATE_CERT_DISTRUST_IMMINENT;
  Assert.ok(isDistrust, "This host should be imminently distrusted");
}

function shouldNotBeImminentlyDistrusted(aTransportSecurityInfo) {
  let isDistrust = aTransportSecurityInfo.securityState &
                     Ci.nsIWebProgressListener.STATE_CERT_DISTRUST_IMMINENT;
  Assert.ok(!isDistrust, "This host should not be imminently distrusted");
}

do_get_profile();

add_tls_server_setup("SymantecSanctionsServer", "test_symantec_apple_google");

// Whitelisted certs aren't to be distrusted
add_connection_test("symantec-whitelist-after-cutoff.example.com",
                    PRErrorCodeSuccess, null, shouldNotBeImminentlyDistrusted);

add_connection_test("symantec-whitelist-before-cutoff.example.com",
                    PRErrorCodeSuccess, null, shouldNotBeImminentlyDistrusted);

// Not-whitelisted certs after the cutoff aren't distrusted
add_connection_test("symantec-not-whitelisted-after-cutoff.example.com",
                    PRErrorCodeSuccess, null, shouldNotBeImminentlyDistrusted);

// Not whitelisted certs before the cutoff are to be distrusted
add_connection_test("symantec-not-whitelisted-before-cutoff.example.com",
                    PRErrorCodeSuccess, null, shouldBeImminentlyDistrusted);
