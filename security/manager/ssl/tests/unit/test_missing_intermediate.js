// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// Tests that if a server does not send a complete certificate chain, we can
// make use of cached intermediates to build a trust path.

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb  = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);

function run_test() {
  addCertFromFile(certdb, "bad_certs/test-ca.pem", "CTu,,");
  add_tls_server_setup("BadCertServer", "bad_certs");
  // If we don't know about the intermediate, we'll get an unknown issuer error.
  add_connection_test("ee-from-missing-intermediate.example.com",
                      SEC_ERROR_UNKNOWN_ISSUER);
  add_test(() => {
    addCertFromFile(certdb, "test_missing_intermediate/missing-intermediate.pem",
                    ",,");
    run_next_test();
  });
  // Now that we've cached the intermediate, the connection should succeed.
  add_connection_test("ee-from-missing-intermediate.example.com",
                      PRErrorCodeSuccess);
  run_next_test();
}
