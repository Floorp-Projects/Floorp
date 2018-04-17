// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests handling of certificates issued by StartCom and WoSign. If such
// certificates have a notBefore before 21 October 2016, they are handled
// normally. Otherwise, they are treated as revoked.

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

function loadCertWithTrust(certName, trustString) {
  addCertFromFile(certdb, "test_startcom_wosign/" + certName + ".pem",
                  trustString);
}

function certFromFile(certName) {
  return constructCertFromFile("test_startcom_wosign/" + certName + ".pem");
}

function checkEndEntity(cert, expectedResult) {
  // (new Date("2016-11-01")).getTime() / 1000
  const VALIDATION_TIME = 1477958400;
  return checkCertErrorGenericAtTime(certdb, cert, expectedResult,
                                     certificateUsageSSLServer,
                                     VALIDATION_TIME);
}

add_task(async function() {
  loadCertWithTrust("ca", "CTu,,");
  // This is not a real StartCom CA - it merely has the same distinguished name
  // as one (namely "/C=IL/O=StartCom Ltd./CN=StartCom Certification Authority
  // G2", encoded with PrintableStrings). By checking for specific DNs, we can
  // enforce the date-based policy in a way that is testable.
  loadCertWithTrust("StartComCA", ",,");
  await checkEndEntity(certFromFile("StartCom-before-cutoff"), PRErrorCodeSuccess);
  await checkEndEntity(certFromFile("StartCom-after-cutoff"), SEC_ERROR_REVOKED_CERTIFICATE);

  // Similarly, this is not a real WoSign CA. It has the same distinguished name
  // as "/C=CN/O=WoSign CA Limited/CN=Certification Authority of WoSign",
  // encoded with PrintableStrings).
  loadCertWithTrust("WoSignCA", ",,");
  await checkEndEntity(certFromFile("WoSign-before-cutoff"), PRErrorCodeSuccess);
  await checkEndEntity(certFromFile("WoSign-after-cutoff"), SEC_ERROR_REVOKED_CERTIFICATE);
});
