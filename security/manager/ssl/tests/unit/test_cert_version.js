// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Tests the interaction between the basic constraints extension and the
// certificate version field. In general, the testcases consist of verifying
// certificate chains of the form:
//
// end-entity (issued by) intermediate (issued by) trusted X509v3 root
//
// where the intermediate is one of X509 v1, v2, v3, or v4, and either does or
// does not have the basic constraints extension. If it has the extension, it
// either does or does not specify that it is a CA.
//
// To test cases where the trust anchor has a different version and/or does or
// does not have the basic constraint extension, there are testcases where the
// intermediate is trusted as an anchor and the verification is repeated.
// (Loading a certificate with trust "CTu,," means that it is a trust anchor
// for SSL. Loading a certificate with trust ",," means that it inherits its
// trust.)
//
// There are also testcases for end-entities issued by a trusted X509v3 root
// where the end-entities similarly cover the range of versions and basic
// constraint extensions.
//
// Finally, there are testcases for self-signed certificates that, again, cover
// the range of versions and basic constraint extensions.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

function certFromFile(certName) {
  return constructCertFromFile("test_cert_version/" + certName + ".pem");
}

function loadCertWithTrust(certName, trustString) {
  addCertFromFile(certdb, "test_cert_version/" + certName + ".pem", trustString);
}

function checkEndEntity(cert, expectedResult) {
  checkCertErrorGeneric(certdb, cert, expectedResult, certificateUsageSSLServer);
}

function checkIntermediate(cert, expectedResult) {
  checkCertErrorGeneric(certdb, cert, expectedResult, certificateUsageSSLCA);
}

function run_test() {
  loadCertWithTrust("ca", "CTu,,");

  // Section for CAs lacking the basicConstraints extension entirely:
  loadCertWithTrust("int-v1-noBC_ca", ",,");
  checkIntermediate(certFromFile("int-v1-noBC_ca"), MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA);
  checkEndEntity(certFromFile("ee_int-v1-noBC"), MOZILLA_PKIX_ERROR_V1_CERT_USED_AS_CA);
  // A v1 certificate with no basicConstraints extension may issue certificates
  // if it is a trust anchor.
  loadCertWithTrust("int-v1-noBC_ca", "CTu,,");
  checkIntermediate(certFromFile("int-v1-noBC_ca"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee_int-v1-noBC"), PRErrorCodeSuccess);

  loadCertWithTrust("int-v2-noBC_ca", ",,");
  checkIntermediate(certFromFile("int-v2-noBC_ca"), SEC_ERROR_CA_CERT_INVALID);
  checkEndEntity(certFromFile("ee_int-v2-noBC"), SEC_ERROR_CA_CERT_INVALID);
  loadCertWithTrust("int-v2-noBC_ca", "CTu,,");
  checkIntermediate(certFromFile("int-v2-noBC_ca"), SEC_ERROR_CA_CERT_INVALID);
  checkEndEntity(certFromFile("ee_int-v2-noBC"), SEC_ERROR_CA_CERT_INVALID);

  loadCertWithTrust("int-v3-noBC_ca", ",,");
  checkIntermediate(certFromFile("int-v3-noBC_ca"), SEC_ERROR_CA_CERT_INVALID);
  checkEndEntity(certFromFile("ee_int-v3-noBC"), SEC_ERROR_CA_CERT_INVALID);
  loadCertWithTrust("int-v3-noBC_ca", "CTu,,");
  checkIntermediate(certFromFile("int-v3-noBC_ca"), SEC_ERROR_CA_CERT_INVALID);
  checkEndEntity(certFromFile("ee_int-v3-noBC"), SEC_ERROR_CA_CERT_INVALID);

  loadCertWithTrust("int-v4-noBC_ca", ",,");
  checkIntermediate(certFromFile("int-v4-noBC_ca"), SEC_ERROR_CA_CERT_INVALID);
  checkEndEntity(certFromFile("ee_int-v4-noBC"), SEC_ERROR_CA_CERT_INVALID);
  loadCertWithTrust("int-v4-noBC_ca", "CTu,,");
  checkIntermediate(certFromFile("int-v4-noBC_ca"), SEC_ERROR_CA_CERT_INVALID);
  checkEndEntity(certFromFile("ee_int-v4-noBC"), SEC_ERROR_CA_CERT_INVALID);

  // Section for CAs with basicConstraints not specifying cA:
  loadCertWithTrust("int-v1-BC-not-cA_ca", ",,");
  checkIntermediate(certFromFile("int-v1-BC-not-cA_ca"), SEC_ERROR_CA_CERT_INVALID);
  checkEndEntity(certFromFile("ee_int-v1-BC-not-cA"), SEC_ERROR_CA_CERT_INVALID);
  loadCertWithTrust("int-v1-BC-not-cA_ca", "CTu,,");
  checkIntermediate(certFromFile("int-v1-BC-not-cA_ca"), SEC_ERROR_CA_CERT_INVALID);
  checkEndEntity(certFromFile("ee_int-v1-BC-not-cA"), SEC_ERROR_CA_CERT_INVALID);

  loadCertWithTrust("int-v2-BC-not-cA_ca", ",,");
  checkIntermediate(certFromFile("int-v2-BC-not-cA_ca"), SEC_ERROR_CA_CERT_INVALID);
  checkEndEntity(certFromFile("ee_int-v2-BC-not-cA"), SEC_ERROR_CA_CERT_INVALID);
  loadCertWithTrust("int-v2-BC-not-cA_ca", "CTu,,");
  checkIntermediate(certFromFile("int-v2-BC-not-cA_ca"), SEC_ERROR_CA_CERT_INVALID);
  checkEndEntity(certFromFile("ee_int-v2-BC-not-cA"), SEC_ERROR_CA_CERT_INVALID);

  loadCertWithTrust("int-v3-BC-not-cA_ca", ",,");
  checkIntermediate(certFromFile("int-v3-BC-not-cA_ca"), SEC_ERROR_CA_CERT_INVALID);
  checkEndEntity(certFromFile("ee_int-v3-BC-not-cA"), SEC_ERROR_CA_CERT_INVALID);
  loadCertWithTrust("int-v3-BC-not-cA_ca", "CTu,,");
  checkIntermediate(certFromFile("int-v3-BC-not-cA_ca"), SEC_ERROR_CA_CERT_INVALID);
  checkEndEntity(certFromFile("ee_int-v3-BC-not-cA"), SEC_ERROR_CA_CERT_INVALID);

  loadCertWithTrust("int-v4-BC-not-cA_ca", ",,");
  checkIntermediate(certFromFile("int-v4-BC-not-cA_ca"), SEC_ERROR_CA_CERT_INVALID);
  checkEndEntity(certFromFile("ee_int-v4-BC-not-cA"), SEC_ERROR_CA_CERT_INVALID);
  loadCertWithTrust("int-v4-BC-not-cA_ca", "CTu,,");
  checkIntermediate(certFromFile("int-v4-BC-not-cA_ca"), SEC_ERROR_CA_CERT_INVALID);
  checkEndEntity(certFromFile("ee_int-v4-BC-not-cA"), SEC_ERROR_CA_CERT_INVALID);

  // Section for CAs with basicConstraints specifying cA:
  loadCertWithTrust("int-v1-BC-cA_ca", ",,");
  checkIntermediate(certFromFile("int-v1-BC-cA_ca"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee_int-v1-BC-cA"), PRErrorCodeSuccess);
  loadCertWithTrust("int-v1-BC-cA_ca", "CTu,,");
  checkIntermediate(certFromFile("int-v1-BC-cA_ca"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee_int-v1-BC-cA"), PRErrorCodeSuccess);

  loadCertWithTrust("int-v2-BC-cA_ca", ",,");
  checkIntermediate(certFromFile("int-v2-BC-cA_ca"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee_int-v2-BC-cA"), PRErrorCodeSuccess);
  loadCertWithTrust("int-v2-BC-cA_ca", "CTu,,");
  checkIntermediate(certFromFile("int-v2-BC-cA_ca"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee_int-v2-BC-cA"), PRErrorCodeSuccess);

  loadCertWithTrust("int-v3-BC-cA_ca", ",,");
  checkIntermediate(certFromFile("int-v3-BC-cA_ca"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee_int-v3-BC-cA"), PRErrorCodeSuccess);
  loadCertWithTrust("int-v3-BC-cA_ca", "CTu,,");
  checkIntermediate(certFromFile("int-v3-BC-cA_ca"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee_int-v3-BC-cA"), PRErrorCodeSuccess);

  loadCertWithTrust("int-v4-BC-cA_ca", ",,");
  checkIntermediate(certFromFile("int-v4-BC-cA_ca"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee_int-v4-BC-cA"), PRErrorCodeSuccess);
  loadCertWithTrust("int-v4-BC-cA_ca", "CTu,,");
  checkIntermediate(certFromFile("int-v4-BC-cA_ca"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee_int-v4-BC-cA"), PRErrorCodeSuccess);

  // Section for end-entity certificates with various basicConstraints:
  checkEndEntity(certFromFile("ee-v1-noBC_ca"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee-v2-noBC_ca"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee-v3-noBC_ca"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee-v4-noBC_ca"), PRErrorCodeSuccess);

  checkEndEntity(certFromFile("ee-v1-BC-not-cA_ca"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee-v2-BC-not-cA_ca"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee-v3-BC-not-cA_ca"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee-v4-BC-not-cA_ca"), PRErrorCodeSuccess);

  checkEndEntity(certFromFile("ee-v1-BC-cA_ca"), MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY);
  checkEndEntity(certFromFile("ee-v2-BC-cA_ca"), MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY);
  checkEndEntity(certFromFile("ee-v3-BC-cA_ca"), MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY);
  checkEndEntity(certFromFile("ee-v4-BC-cA_ca"), MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY);

  // Section for self-signed certificates:
  checkEndEntity(certFromFile("ss-v1-noBC"), SEC_ERROR_UNKNOWN_ISSUER);
  checkEndEntity(certFromFile("ss-v2-noBC"), SEC_ERROR_UNKNOWN_ISSUER);
  checkEndEntity(certFromFile("ss-v3-noBC"), SEC_ERROR_UNKNOWN_ISSUER);
  checkEndEntity(certFromFile("ss-v4-noBC"), SEC_ERROR_UNKNOWN_ISSUER);

  checkEndEntity(certFromFile("ss-v1-BC-not-cA"), SEC_ERROR_UNKNOWN_ISSUER);
  checkEndEntity(certFromFile("ss-v2-BC-not-cA"), SEC_ERROR_UNKNOWN_ISSUER);
  checkEndEntity(certFromFile("ss-v3-BC-not-cA"), SEC_ERROR_UNKNOWN_ISSUER);
  checkEndEntity(certFromFile("ss-v4-BC-not-cA"), SEC_ERROR_UNKNOWN_ISSUER);

  checkEndEntity(certFromFile("ss-v1-BC-cA"), SEC_ERROR_UNKNOWN_ISSUER);
  checkEndEntity(certFromFile("ss-v2-BC-cA"), SEC_ERROR_UNKNOWN_ISSUER);
  checkEndEntity(certFromFile("ss-v3-BC-cA"), SEC_ERROR_UNKNOWN_ISSUER);
  checkEndEntity(certFromFile("ss-v4-BC-cA"), SEC_ERROR_UNKNOWN_ISSUER);
}
