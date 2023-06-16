// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const gCertDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

function certFromFile(certName) {
  return constructCertFromFile(`test_baseline_requirements/${certName}.pem`);
}

function loadCertWithTrust(certName, trustString) {
  addCertFromFile(
    gCertDB,
    `test_baseline_requirements/${certName}.pem`,
    trustString
  );
}

function checkCertOn25August2016(cert, expectedResult) {
  // (new Date("2016-08-25T00:00:00Z")).getTime() / 1000
  const VALIDATION_TIME = 1472083200;
  return checkCertErrorGenericAtTime(
    gCertDB,
    cert,
    expectedResult,
    certificateUsageSSLServer,
    VALIDATION_TIME,
    false,
    "example.com"
  );
}

add_task(async function () {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("privacy.reduceTimerPrecision");
  });

  Services.prefs.setBoolPref("privacy.reduceTimerPrecision", false);

  loadCertWithTrust("ca", "CTu,,");

  // At one time there was a preference security.pki.name_matching_mode that
  // controlled whether or not mozilla::pkix would fall back to using a
  // certificate's subject common name during name matching. This no longer
  // exists, and certificates that previously required the fallback should fail
  // to verify.

  await checkCertOn25August2016(
    certFromFile("no-san-recent"),
    SSL_ERROR_BAD_CERT_DOMAIN
  );
  await checkCertOn25August2016(
    certFromFile("no-san-old"),
    SSL_ERROR_BAD_CERT_DOMAIN
  );
  await checkCertOn25August2016(
    certFromFile("no-san-older"),
    SSL_ERROR_BAD_CERT_DOMAIN
  );
  await checkCertOn25August2016(
    certFromFile("san-contains-no-hostnames-recent"),
    SSL_ERROR_BAD_CERT_DOMAIN
  );
  await checkCertOn25August2016(
    certFromFile("san-contains-no-hostnames-old"),
    SSL_ERROR_BAD_CERT_DOMAIN
  );
  await checkCertOn25August2016(
    certFromFile("san-contains-no-hostnames-older"),
    SSL_ERROR_BAD_CERT_DOMAIN
  );
});
