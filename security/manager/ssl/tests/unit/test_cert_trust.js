// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb  = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);

function load_cert(cert_name, trust_string) {
  let cert_filename = cert_name + ".pem";
  return addCertFromFile(certdb, "test_cert_trust/" + cert_filename,
                         trust_string);
}

function setup_basic_trusts(ca_cert, int_cert) {
  certdb.setCertTrust(ca_cert, Ci.nsIX509Cert.CA_CERT,
                      Ci.nsIX509CertDB.TRUSTED_SSL |
                      Ci.nsIX509CertDB.TRUSTED_EMAIL);

  certdb.setCertTrust(int_cert, Ci.nsIX509Cert.CA_CERT, 0);
}

function test_ca_distrust(ee_cert, cert_to_modify_trust, isRootCA) {
  // On reset most usages are successful
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageSSLServer);
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageSSLClient);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_CA_CERT_INVALID,
                        certificateUsageSSLCA);
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageEmailSigner);
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageEmailRecipient);


  // Test of active distrust. No usage should pass.
  setCertTrust(cert_to_modify_trust, "p,p,p");
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_UNTRUSTED_ISSUER,
                        certificateUsageSSLServer);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_UNTRUSTED_ISSUER,
                        certificateUsageSSLClient);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_CA_CERT_INVALID,
                        certificateUsageSSLCA);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_UNTRUSTED_ISSUER,
                        certificateUsageEmailSigner);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_UNTRUSTED_ISSUER,
                        certificateUsageEmailRecipient);

  // Trust set to T  -  trusted CA to issue client certs, where client cert is
  // usageSSLClient.
  setCertTrust(cert_to_modify_trust, "T,T,T");
  checkCertErrorGeneric(certdb, ee_cert, isRootCA ? SEC_ERROR_UNKNOWN_ISSUER
                                                  : PRErrorCodeSuccess,
                        certificateUsageSSLServer);

  // XXX(Bug 982340)
  checkCertErrorGeneric(certdb, ee_cert, isRootCA ? SEC_ERROR_UNKNOWN_ISSUER
                                                  : PRErrorCodeSuccess,
                        certificateUsageSSLClient);

  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_CA_CERT_INVALID,
                        certificateUsageSSLCA);

  checkCertErrorGeneric(certdb, ee_cert, isRootCA ? SEC_ERROR_UNKNOWN_ISSUER
                                                  : PRErrorCodeSuccess,
                        certificateUsageEmailSigner);
  checkCertErrorGeneric(certdb, ee_cert, isRootCA ? SEC_ERROR_UNKNOWN_ISSUER
                                                  : PRErrorCodeSuccess,
                        certificateUsageEmailRecipient);


  // Now tests on the SSL trust bit
  setCertTrust(cert_to_modify_trust, "p,C,C");
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_UNTRUSTED_ISSUER,
                        certificateUsageSSLServer);

  // XXX(Bug 982340)
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageSSLClient);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_CA_CERT_INVALID,
                        certificateUsageSSLCA);
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageEmailSigner);
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageEmailRecipient);

  // Inherited trust SSL
  setCertTrust(cert_to_modify_trust, ",C,C");
  checkCertErrorGeneric(certdb, ee_cert, isRootCA ? SEC_ERROR_UNKNOWN_ISSUER
                                                  : PRErrorCodeSuccess,
                        certificateUsageSSLServer);
  // XXX(Bug 982340)
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageSSLClient);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_CA_CERT_INVALID,
                        certificateUsageSSLCA);
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageEmailSigner);
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageEmailRecipient);

  // Now tests on the EMAIL trust bit
  setCertTrust(cert_to_modify_trust, "C,p,C");
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageSSLServer);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_UNTRUSTED_ISSUER,
                        certificateUsageSSLClient);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_CA_CERT_INVALID,
                        certificateUsageSSLCA);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_UNTRUSTED_ISSUER,
                        certificateUsageEmailSigner);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_UNTRUSTED_ISSUER,
                        certificateUsageEmailRecipient);


  // inherited EMAIL Trust
  setCertTrust(cert_to_modify_trust, "C,,C");
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageSSLServer);
  checkCertErrorGeneric(certdb, ee_cert, isRootCA ? SEC_ERROR_UNKNOWN_ISSUER
                                                  : PRErrorCodeSuccess,
                        certificateUsageSSLClient);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_CA_CERT_INVALID,
                        certificateUsageSSLCA);
  checkCertErrorGeneric(certdb, ee_cert, isRootCA ? SEC_ERROR_UNKNOWN_ISSUER
                                                  : PRErrorCodeSuccess,
                        certificateUsageEmailSigner);
  checkCertErrorGeneric(certdb, ee_cert, isRootCA ? SEC_ERROR_UNKNOWN_ISSUER
                                                  : PRErrorCodeSuccess,
                        certificateUsageEmailRecipient);
}


function run_test() {
  let certList = [
    "ca",
    "int",
    "ee",
  ];
  let loadedCerts = [];
  for (let certName of certList) {
    loadedCerts.push(load_cert(certName, ",,"));
  }

  let ca_cert = loadedCerts[0];
  notEqual(ca_cert, null, "CA cert should have successfully loaded");
  let int_cert = loadedCerts[1];
  notEqual(int_cert, null, "Intermediate cert should have successfully loaded");
  let ee_cert = loadedCerts[2];
  notEqual(ee_cert, null, "EE cert should have successfully loaded");

  setup_basic_trusts(ca_cert, int_cert);
  test_ca_distrust(ee_cert, ca_cert, true);

  setup_basic_trusts(ca_cert, int_cert);
  test_ca_distrust(ee_cert, int_cert, false);

  // Reset trust to default ("inherit trust")
  setCertTrust(ca_cert, ",,");
  setCertTrust(int_cert, ",,");

  // If an end-entity certificate is manually trusted, it may not be the root of
  // its own verified chain. In general this will cause "unknown issuer" errors
  // unless a CA trust anchor can be found.
  setCertTrust(ee_cert, "CTu,CTu,CTu");
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_UNKNOWN_ISSUER,
                        certificateUsageSSLServer);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_UNKNOWN_ISSUER,
                        certificateUsageSSLClient);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_UNKNOWN_ISSUER,
                        certificateUsageEmailSigner);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_UNKNOWN_ISSUER,
                        certificateUsageEmailRecipient);

  // Now make a CA trust anchor available.
  setCertTrust(ca_cert, "CTu,CTu,CTu");
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageSSLServer);
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageSSLClient);
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageEmailSigner);
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageEmailRecipient);
}
