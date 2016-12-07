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
                      Ci.nsIX509CertDB.TRUSTED_EMAIL |
                      Ci.nsIX509CertDB.TRUSTED_OBJSIGN);

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
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageObjectSigner);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_CA_CERT_INVALID,
                        certificateUsageVerifyCA);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_INADEQUATE_CERT_TYPE,
                        certificateUsageStatusResponder);


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
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_UNTRUSTED_ISSUER,
                        certificateUsageObjectSigner);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_CA_CERT_INVALID,
                        certificateUsageVerifyCA);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_UNTRUSTED_ISSUER,
                        certificateUsageStatusResponder);

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
  checkCertErrorGeneric(certdb, ee_cert, isRootCA ? SEC_ERROR_UNKNOWN_ISSUER
                                                  : PRErrorCodeSuccess,
                        certificateUsageObjectSigner);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_CA_CERT_INVALID,
                        certificateUsageVerifyCA);
  checkCertErrorGeneric(certdb, ee_cert,
                        isRootCA ? SEC_ERROR_UNKNOWN_ISSUER
                                 : SEC_ERROR_INADEQUATE_CERT_TYPE,
                        certificateUsageStatusResponder);


  // Now tests on the SSL trust bit
  setCertTrust(cert_to_modify_trust, "p,C,C");
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_UNTRUSTED_ISSUER,
                        certificateUsageSSLServer);

  //XXX(Bug 982340)
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageSSLClient);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_CA_CERT_INVALID,
                        certificateUsageSSLCA);
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageEmailSigner);
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageEmailRecipient);
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageObjectSigner);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_CA_CERT_INVALID,
                        certificateUsageVerifyCA);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_UNTRUSTED_ISSUER,
                        certificateUsageStatusResponder);

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
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageObjectSigner);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_CA_CERT_INVALID,
                        certificateUsageVerifyCA);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_INADEQUATE_CERT_TYPE,
                        certificateUsageStatusResponder);

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
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageObjectSigner);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_CA_CERT_INVALID,
                        certificateUsageVerifyCA);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_INADEQUATE_CERT_TYPE,
                        certificateUsageStatusResponder);


  //inherited EMAIL Trust
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
  checkCertErrorGeneric(certdb, ee_cert, PRErrorCodeSuccess,
                        certificateUsageObjectSigner);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_CA_CERT_INVALID,
                        certificateUsageVerifyCA);
  checkCertErrorGeneric(certdb, ee_cert, SEC_ERROR_INADEQUATE_CERT_TYPE,
                        certificateUsageStatusResponder);
}


function run_test() {
  let certList = [
    "ca",
    "int",
    "ee",
  ];
  let loadedCerts = [];
  for (let i = 0 ; i < certList.length; i++) {
    loadedCerts.push(load_cert(certList[i], ",,"));
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
}
