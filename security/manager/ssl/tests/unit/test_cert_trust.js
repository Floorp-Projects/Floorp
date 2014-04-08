// -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb  = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);

let certList = [
  'ee',
  'int',
  'ca',
]

function load_cert(cert_name, trust_string) {
  let cert_filename = cert_name + ".der";
  addCertFromFile(certdb, "test_cert_trust/" + cert_filename, trust_string);
}

function setup_basic_trusts(ca_cert, int_cert) {
  certdb.setCertTrust(ca_cert, Ci.nsIX509Cert.CA_CERT,
                      Ci.nsIX509CertDB.TRUSTED_SSL |
                      Ci.nsIX509CertDB.TRUSTED_EMAIL |
                      Ci.nsIX509CertDB.TRUSTED_OBJSIGN);

  certdb.setCertTrust(int_cert, Ci.nsIX509Cert.CA_CERT, 0);
}

function check_cert_err_generic(cert, expected_error, usage) {
  do_print("cert cn=" + cert.commonName);
  do_print("cert issuer cn=" + cert.issuerCommonName);
  let hasEVPolicy = {};
  let verifiedChain = {};
  let error = certdb.verifyCertNow(cert, usage,
                                   NO_FLAGS, verifiedChain, hasEVPolicy);
  do_check_eq(error,  expected_error);
};

function test_ca_distrust(ee_cert, cert_to_modify_trust, isRootCA, useMozillaPKIX) {
  // On reset most usages are successful
  check_cert_err_generic(ee_cert, 0, certificateUsageSSLServer);
  check_cert_err_generic(ee_cert, 0, certificateUsageSSLClient);
  check_cert_err_generic(ee_cert, useMozillaPKIX ? SEC_ERROR_CA_CERT_INVALID
                                                 : SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageSSLCA);  // expected no bc
  check_cert_err_generic(ee_cert, 0, certificateUsageEmailSigner);
  check_cert_err_generic(ee_cert, 0, certificateUsageEmailRecipient);
  check_cert_err_generic(ee_cert, useMozillaPKIX ? 0
                                                 : SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageObjectSigner); // expected
  check_cert_err_generic(ee_cert, useMozillaPKIX ? SEC_ERROR_CA_CERT_INVALID
                                                 : 0,
                         certificateUsageVerifyCA);
  // mozilla::pkix enforces that certificase must have a basic constraints
  // extension with cA:true to be a CA certificate,  whereas classic does not
  check_cert_err_generic(ee_cert, SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageStatusResponder); //expected


  // Test of active distrust. No usage should pass.
  setCertTrust(cert_to_modify_trust, 'p,p,p');
  check_cert_err_generic(ee_cert, SEC_ERROR_UNTRUSTED_ISSUER,
                         certificateUsageSSLServer);
  check_cert_err_generic(ee_cert, SEC_ERROR_UNTRUSTED_ISSUER,
                         certificateUsageSSLClient);
  check_cert_err_generic(ee_cert, useMozillaPKIX ? SEC_ERROR_CA_CERT_INVALID
                                                 : SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageSSLCA);
  check_cert_err_generic(ee_cert, SEC_ERROR_UNTRUSTED_ISSUER,
                         certificateUsageEmailSigner);
  check_cert_err_generic(ee_cert, SEC_ERROR_UNTRUSTED_ISSUER,
                         certificateUsageEmailRecipient);
  check_cert_err_generic(ee_cert, useMozillaPKIX ? SEC_ERROR_UNTRUSTED_ISSUER
                                                 : SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageObjectSigner);
  check_cert_err_generic(ee_cert, useMozillaPKIX ? SEC_ERROR_CA_CERT_INVALID
                                                 : 0,
                         certificateUsageVerifyCA);
  // In mozilla::pkix (but not classic verification), certificate chain
  // properties are checked before the end-entity. Thus, if we're using
  // mozilla::pkix and the root certificate has been distrusted, the error
  // will be "untrusted issuer" and not "inadequate cert type".
  check_cert_err_generic(ee_cert, (!isRootCA && useMozillaPKIX)
                                    ? SEC_ERROR_UNTRUSTED_ISSUER
                                    : SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageStatusResponder);


  // Trust set to T  -  trusted CA to issue client certs, where client cert is
  // usageSSLClient.
  setCertTrust(cert_to_modify_trust, 'T,T,T');
  check_cert_err_generic(ee_cert, isRootCA ? useMozillaPKIX ? SEC_ERROR_UNKNOWN_ISSUER
                                                            : SEC_ERROR_UNTRUSTED_ISSUER
                                           : 0,
                         certificateUsageSSLServer);

  check_cert_err_generic(ee_cert, isRootCA ? useMozillaPKIX ? SEC_ERROR_UNKNOWN_ISSUER //XXX Bug 982340
                                                            : 0
                                           : 0,
                         certificateUsageSSLClient);
  check_cert_err_generic(ee_cert, useMozillaPKIX ? SEC_ERROR_CA_CERT_INVALID
                                                 : SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageSSLCA);

  check_cert_err_generic(ee_cert, isRootCA ? useMozillaPKIX ? SEC_ERROR_UNKNOWN_ISSUER
                                                            : SEC_ERROR_UNTRUSTED_ISSUER
                                           : 0,
                         certificateUsageEmailSigner);
  check_cert_err_generic(ee_cert, isRootCA ? useMozillaPKIX ? SEC_ERROR_UNKNOWN_ISSUER
                                                            : SEC_ERROR_UNTRUSTED_ISSUER
                                           : 0,
                         certificateUsageEmailRecipient);
  check_cert_err_generic(ee_cert, isRootCA ? useMozillaPKIX ? SEC_ERROR_UNKNOWN_ISSUER
                                                            : SEC_ERROR_INADEQUATE_CERT_TYPE
                                           : useMozillaPKIX ? 0
                                                            : SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageObjectSigner);
  check_cert_err_generic(ee_cert, useMozillaPKIX ? SEC_ERROR_CA_CERT_INVALID
                                                 : 0,
                         certificateUsageVerifyCA);
  check_cert_err_generic(ee_cert, SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageStatusResponder);


  // Now tests on the SSL trust bit
  setCertTrust(cert_to_modify_trust, 'p,C,C');
  check_cert_err_generic(ee_cert, SEC_ERROR_UNTRUSTED_ISSUER,
                         certificateUsageSSLServer);
  check_cert_err_generic(ee_cert, useMozillaPKIX ? 0  //XXX Bug 982340
                                                 : SEC_ERROR_UNTRUSTED_ISSUER,
                         certificateUsageSSLClient);
  check_cert_err_generic(ee_cert, useMozillaPKIX ? SEC_ERROR_CA_CERT_INVALID
                                                 : SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageSSLCA);
  check_cert_err_generic(ee_cert, 0, certificateUsageEmailSigner);
  check_cert_err_generic(ee_cert, 0, certificateUsageEmailRecipient);
  check_cert_err_generic(ee_cert, useMozillaPKIX ? 0
                                                 : SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageObjectSigner);
  check_cert_err_generic(ee_cert, useMozillaPKIX ? SEC_ERROR_CA_CERT_INVALID
                                                 : 0,
                         certificateUsageVerifyCA);
  // In mozilla::pkix (but not classic verification), certificate chain
  // properties are checked before the end-entity. Thus, if we're using
  // mozilla::pkix and the root certificate has been distrusted, the error
  // will be "untrusted issuer" and not "inadequate cert type".
  check_cert_err_generic(ee_cert, (!isRootCA && useMozillaPKIX)
                                    ? SEC_ERROR_UNTRUSTED_ISSUER
                                    : SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageStatusResponder);

  // Inherited trust SSL
  setCertTrust(cert_to_modify_trust, ',C,C');
  check_cert_err_generic(ee_cert, isRootCA ? useMozillaPKIX ? SEC_ERROR_UNKNOWN_ISSUER
                                                            : SEC_ERROR_UNTRUSTED_ISSUER
                                           : 0,
                         certificateUsageSSLServer);
  check_cert_err_generic(ee_cert, isRootCA ? useMozillaPKIX ? 0  // XXX Bug 982340
                                                            : SEC_ERROR_UNTRUSTED_ISSUER
                                           : 0,
                         certificateUsageSSLClient);
  check_cert_err_generic(ee_cert, useMozillaPKIX ? SEC_ERROR_CA_CERT_INVALID
                                                 : SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageSSLCA);
  check_cert_err_generic(ee_cert, 0, certificateUsageEmailSigner);
  check_cert_err_generic(ee_cert, 0, certificateUsageEmailRecipient);
  check_cert_err_generic(ee_cert, useMozillaPKIX ? 0
                                                 : SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageObjectSigner);
  check_cert_err_generic(ee_cert, useMozillaPKIX ? SEC_ERROR_CA_CERT_INVALID
                                                 : 0,
                         certificateUsageVerifyCA);
  check_cert_err_generic(ee_cert, SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageStatusResponder);

  // Now tests on the EMAIL trust bit
  setCertTrust(cert_to_modify_trust, 'C,p,C');
  check_cert_err_generic(ee_cert, 0, certificateUsageSSLServer);
  check_cert_err_generic(ee_cert, isRootCA ? SEC_ERROR_UNTRUSTED_ISSUER
                                           : useMozillaPKIX ? SEC_ERROR_UNTRUSTED_ISSUER
                                                            : 0, // mozilla::pkix is OK, NSS bug
                         certificateUsageSSLClient);
  check_cert_err_generic(ee_cert, useMozillaPKIX ? SEC_ERROR_CA_CERT_INVALID
                                                 : SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageSSLCA);
  check_cert_err_generic(ee_cert, SEC_ERROR_UNTRUSTED_ISSUER,
                         certificateUsageEmailSigner);
  check_cert_err_generic(ee_cert, SEC_ERROR_UNTRUSTED_ISSUER,
                         certificateUsageEmailRecipient);
  check_cert_err_generic(ee_cert, useMozillaPKIX ? 0
                                                 : SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageObjectSigner);
  check_cert_err_generic(ee_cert, useMozillaPKIX ? SEC_ERROR_CA_CERT_INVALID
                                                 : 0,
                         certificateUsageVerifyCA);
  check_cert_err_generic(ee_cert, SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageStatusResponder);


  //inherited EMAIL Trust
  setCertTrust(cert_to_modify_trust, 'C,,C');
  check_cert_err_generic(ee_cert, 0, certificateUsageSSLServer);
  check_cert_err_generic(ee_cert, isRootCA ? useMozillaPKIX ? SEC_ERROR_UNKNOWN_ISSUER
                                                            : SEC_ERROR_UNTRUSTED_ISSUER
                                           : 0,
                         certificateUsageSSLClient);
  check_cert_err_generic(ee_cert, useMozillaPKIX ? SEC_ERROR_CA_CERT_INVALID
                                                 : SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageSSLCA);
  check_cert_err_generic(ee_cert, isRootCA ? useMozillaPKIX ? SEC_ERROR_UNKNOWN_ISSUER
                                                            : SEC_ERROR_UNTRUSTED_ISSUER
                                           : 0,
                         certificateUsageEmailSigner);
  check_cert_err_generic(ee_cert, isRootCA ? useMozillaPKIX ? SEC_ERROR_UNKNOWN_ISSUER
                                                            : SEC_ERROR_UNTRUSTED_ISSUER
                                           : 0,
                         certificateUsageEmailRecipient);
  check_cert_err_generic(ee_cert, useMozillaPKIX ? 0
                                                 : SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageObjectSigner);
  check_cert_err_generic(ee_cert, useMozillaPKIX ? SEC_ERROR_CA_CERT_INVALID
                                                 : 0,
                         certificateUsageVerifyCA);
  check_cert_err_generic(ee_cert, SEC_ERROR_INADEQUATE_CERT_TYPE,
                         certificateUsageStatusResponder);
}


function run_test_in_mode(useMozillaPKIX) {
  Services.prefs.setBoolPref("security.use_mozillapkix_verification", useMozillaPKIX);

  let ca_cert = certdb.findCertByNickname(null, 'ca');
  do_check_false(!ca_cert)
  let int_cert = certdb.findCertByNickname(null, 'int');
  do_check_false(!int_cert)
  let ee_cert = certdb.findCertByNickname(null, 'ee');
  do_check_false(!ee_cert);

  setup_basic_trusts(ca_cert, int_cert);
  test_ca_distrust(ee_cert, ca_cert, true, useMozillaPKIX);

  setup_basic_trusts(ca_cert, int_cert);
  test_ca_distrust(ee_cert, int_cert, false, useMozillaPKIX);
}

function run_test() {
  for (let i = 0 ; i < certList.length; i++) {
    load_cert(certList[i], ',,');
  }

  run_test_in_mode(true);
  run_test_in_mode(false);
}
