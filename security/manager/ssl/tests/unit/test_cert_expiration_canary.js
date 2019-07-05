// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// Attempts to verify a certificate for a time a few weeks into the future in
// the hopes of avoiding mass test failures when the certificates all expire.
// If this test fails, the certificates probably need to be regenerated.
// See bug 1525191.
add_task(async function() {
  do_get_profile();
  let certDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certDB, "bad_certs/test-ca.pem", "CTu,,");
  let threeWeeksFromNowInSeconds = Date.now() / 1000 + 3 * 7 * 24 * 60 * 60;
  let ee = constructCertFromFile("bad_certs/default-ee.pem");
  await checkCertErrorGenericAtTime(
    certDB,
    ee,
    PRErrorCodeSuccess,
    certificateUsageSSLServer,
    threeWeeksFromNowInSeconds,
    false,
    "test.example.com"
  );
});
