// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Tests the rejection of SHA-1 certificates.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

// (new Date("2016-03-01")).getTime() / 1000
const VALIDATION_TIME = 1456790400;

function certFromFile(certName) {
  return constructCertFromFile("test_cert_sha1/" + certName + ".pem");
}

function loadCertWithTrust(certName, trustString) {
  addCertFromFile(certdb, "test_cert_sha1/" + certName + ".pem", trustString);
}

function checkEndEntity(cert, expectedResult) {
  return checkCertErrorGenericAtTime(
    certdb,
    cert,
    expectedResult,
    certificateUsageSSLServer,
    VALIDATION_TIME
  );
}

add_task(async function () {
  loadCertWithTrust("ca", "CTu,,");
  loadCertWithTrust("int-pre", ",,");
  loadCertWithTrust("int-post", ",,");

  await checkEndEntity(
    certFromFile("ee-pre_int-pre"),
    SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED
  );
  await checkEndEntity(
    certFromFile("ee-post_int-pre"),
    SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED
  );
  await checkEndEntity(
    certFromFile("ee-post_int-post"),
    SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED
  );
});
