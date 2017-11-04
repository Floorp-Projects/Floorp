/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Tests handling of certificates issued by Symantec. If such
// certificates have a notBefore before 1 June 2016, and are not
// issued by an Apple or Google intermediate, they should emit a
// warning to the console.

function shouldNotBeImminentlyDistrusted(aTransportSecurityInfo) {
  let isDistrust = aTransportSecurityInfo.securityState &
                     Ci.nsIWebProgressListener.STATE_CERT_DISTRUST_IMMINENT;
  Assert.ok(!isDistrust, "This host should not be imminently distrusted");
}

do_get_profile();

add_tls_server_setup("OCSPStaplingServer", "ocsp_certs");

add_connection_test("ocsp-stapling-good.example.com",
                    PRErrorCodeSuccess, null, shouldNotBeImminentlyDistrusted);
