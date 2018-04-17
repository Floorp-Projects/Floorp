// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Tests that the extended key usage extension is properly processed by the
// platform when verifying certificates. There are already comprehensive tests
// in mozilla::pkix itself, but these tests serve as integration tests to ensure
// that the cases we're particularly concerned about are correctly handled.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

function certFromFile(certName) {
  return constructCertFromFile(`test_cert_eku/${certName}.pem`);
}

function loadCertWithTrust(certName, trustString) {
  addCertFromFile(certdb, `test_cert_eku/${certName}.pem`, trustString);
}

function checkEndEntity(cert, expectedResult) {
  return checkCertErrorGeneric(certdb, cert, expectedResult,
                               certificateUsageSSLServer);
}

function checkCertOn25August2016(cert, expectedResult) {
  // (new Date("2016-08-25T00:00:00Z")).getTime() / 1000
  const VALIDATION_TIME = 1472083200;
  return checkCertErrorGenericAtTime(certdb, cert, expectedResult,
                                     certificateUsageSSLServer,
                                     VALIDATION_TIME);
}


add_task(async function() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("privacy.reduceTimerPrecision");
  });
  Services.prefs.setBoolPref("privacy.reduceTimerPrecision", false);

  loadCertWithTrust("ca", "CTu,,");
  // end-entity has id-kp-serverAuth => success
  await checkEndEntity(certFromFile("ee-SA"), PRErrorCodeSuccess);
  // end-entity has id-kp-serverAuth => success
  await checkEndEntity(certFromFile("ee-SA-CA"), PRErrorCodeSuccess);
  // end-entity has extended key usage, but id-kp-serverAuth is not present =>
  // failure
  await checkEndEntity(certFromFile("ee-CA"), SEC_ERROR_INADEQUATE_CERT_TYPE);
  // end-entity has id-kp-serverAuth => success
  await checkEndEntity(certFromFile("ee-SA-nsSGC"), PRErrorCodeSuccess);

  // end-entity has extended key usage, but id-kp-serverAuth is not present =>
  // failure (in particular, Netscape Server Gated Crypto (also known as
  // Netscape Step Up) is not an acceptable substitute for end-entity
  // certificates).
  // Verify this for all Netscape Step Up policy configurations.
  // 0 = "always accept nsSGC in place of serverAuth for CA certificates"
  Services.prefs.setIntPref("security.pki.netscape_step_up_policy", 0);
  await checkEndEntity(certFromFile("ee-nsSGC"), SEC_ERROR_INADEQUATE_CERT_TYPE);
  // 1 = "accept nsSGC before 23 August 2016"
  Services.prefs.setIntPref("security.pki.netscape_step_up_policy", 1);
  await checkEndEntity(certFromFile("ee-nsSGC"), SEC_ERROR_INADEQUATE_CERT_TYPE);
  // 2 = "accept nsSGC before 23 August 2015"
  Services.prefs.setIntPref("security.pki.netscape_step_up_policy", 2);
  await checkEndEntity(certFromFile("ee-nsSGC"), SEC_ERROR_INADEQUATE_CERT_TYPE);
  // 3 = "never accept nsSGC"
  Services.prefs.setIntPref("security.pki.netscape_step_up_policy", 3);
  await checkEndEntity(certFromFile("ee-nsSGC"), SEC_ERROR_INADEQUATE_CERT_TYPE);

  // end-entity has id-kp-OCSPSigning, which is not acceptable for end-entity
  // certificates being verified as TLS server certificates => failure
  await checkEndEntity(certFromFile("ee-SA-OCSP"), SEC_ERROR_INADEQUATE_CERT_TYPE);

  // intermediate has id-kp-serverAuth => success
  loadCertWithTrust("int-SA", ",,");
  await checkEndEntity(certFromFile("ee-int-SA"), PRErrorCodeSuccess);
  // intermediate has id-kp-serverAuth => success
  loadCertWithTrust("int-SA-CA", ",,");
  await checkEndEntity(certFromFile("ee-int-SA-CA"), PRErrorCodeSuccess);
  // intermediate has extended key usage, but id-kp-serverAuth is not present
  // => failure
  loadCertWithTrust("int-CA", ",,");
  await checkEndEntity(certFromFile("ee-int-CA"), SEC_ERROR_INADEQUATE_CERT_TYPE);
  // intermediate has id-kp-serverAuth => success
  loadCertWithTrust("int-SA-nsSGC", ",,");
  await checkEndEntity(certFromFile("ee-int-SA-nsSGC"), PRErrorCodeSuccess);

  // Intermediate has Netscape Server Gated Crypto. Success will depend on the
  // Netscape Step Up policy configuration and the notBefore property of the
  // intermediate.
  loadCertWithTrust("int-nsSGC-recent", ",,");
  loadCertWithTrust("int-nsSGC-old", ",,");
  loadCertWithTrust("int-nsSGC-older", ",,");
  // 0 = "always accept nsSGC in place of serverAuth for CA certificates"
  Services.prefs.setIntPref("security.pki.netscape_step_up_policy", 0);
  info("Netscape Step Up policy: always accept");
  await checkCertOn25August2016(certFromFile("ee-int-nsSGC-recent"),
                          PRErrorCodeSuccess);
  await checkCertOn25August2016(certFromFile("ee-int-nsSGC-old"),
                          PRErrorCodeSuccess);
  await checkCertOn25August2016(certFromFile("ee-int-nsSGC-older"),
                          PRErrorCodeSuccess);
  // 1 = "accept nsSGC before 23 August 2016"
  info("Netscape Step Up policy: accept before 23 August 2016");
  Services.prefs.setIntPref("security.pki.netscape_step_up_policy", 1);
  await checkCertOn25August2016(certFromFile("ee-int-nsSGC-recent"),
                          SEC_ERROR_INADEQUATE_CERT_TYPE);
  await checkCertOn25August2016(certFromFile("ee-int-nsSGC-old"),
                          PRErrorCodeSuccess);
  await checkCertOn25August2016(certFromFile("ee-int-nsSGC-older"),
                          PRErrorCodeSuccess);
  // 2 = "accept nsSGC before 23 August 2015"
  info("Netscape Step Up policy: accept before 23 August 2015");
  Services.prefs.setIntPref("security.pki.netscape_step_up_policy", 2);
  await checkCertOn25August2016(certFromFile("ee-int-nsSGC-recent"),
                          SEC_ERROR_INADEQUATE_CERT_TYPE);
  await checkCertOn25August2016(certFromFile("ee-int-nsSGC-old"),
                          SEC_ERROR_INADEQUATE_CERT_TYPE);
  await checkCertOn25August2016(certFromFile("ee-int-nsSGC-older"),
                          PRErrorCodeSuccess);
  // 3 = "never accept nsSGC"
  info("Netscape Step Up policy: never accept");
  Services.prefs.setIntPref("security.pki.netscape_step_up_policy", 3);
  await checkCertOn25August2016(certFromFile("ee-int-nsSGC-recent"),
                          SEC_ERROR_INADEQUATE_CERT_TYPE);
  await checkCertOn25August2016(certFromFile("ee-int-nsSGC-old"),
                          SEC_ERROR_INADEQUATE_CERT_TYPE);
  await checkCertOn25August2016(certFromFile("ee-int-nsSGC-older"),
                          SEC_ERROR_INADEQUATE_CERT_TYPE);

  // intermediate has id-kp-OCSPSigning, which is acceptable for CA
  // certificates => success
  loadCertWithTrust("int-SA-OCSP", ",,");
  await checkEndEntity(certFromFile("ee-int-SA-OCSP"), PRErrorCodeSuccess);
});
