// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// Attempts to verify a certificate for a time a few weeks into the future in
// the hopes of avoiding mass test failures when the certificates all expire.
// If this test fails, the certificates probably need to be regenerated.
// See bug 1525191.

// If this test and only this test fails, do the following:
// 1. Create a bug for the issue in "Core :: Security: PSM".
// 2. Write a patch to temporarily disable the test.
// 3. Land the patch.
// 4. Write a patch to reenable the test but don't land it.
// 5. Needinfo the triage owner of Bugzilla's "Core :: Security: PSM" component
//    in the bug.
// 6. Patches to update certificates get created.
// 7. Test the patches with a Try push.
// 8. Land the patches on all trees whose code will still be used when the
//    certificates expire in 3 weeks.
add_task(async function () {
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
