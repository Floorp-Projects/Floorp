// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// Checks that RSA and DSA certs with key sizes below 1024 bits are rejected.

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

function certFromFile(filename) {
  let der = readFile(do_get_file("test_keysize/" + filename, false));
  return certdb.constructX509(der, der.length);
}

function load_cert(cert_name, trust_string) {
  let cert_filename = cert_name + ".der";
  addCertFromFile(certdb, "test_keysize/" + cert_filename, trust_string);
  return certFromFile(cert_filename);
}

function check_cert_err_generic(cert, expected_error, usage) {
  do_print("cert cn=" + cert.commonName);
  do_print("cert issuer cn=" + cert.issuerCommonName);
  let hasEVPolicy = {};
  let verifiedChain = {};
  let error = certdb.verifyCertNow(cert, usage,
                                   NO_FLAGS, verifiedChain, hasEVPolicy);
  equal(error, expected_error);
}

function check_cert_err(cert, expected_error) {
  check_cert_err_generic(cert, expected_error, certificateUsageSSLServer)
}

function check_ok(cert) {
  return check_cert_err(cert, 0);
}

function check_ok_ca(cert) {
  return check_cert_err_generic(cert, 0, certificateUsageSSLCA);
}

function check_fail(cert) {
  return check_cert_err(cert, MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE);
}

function check_fail_ca(cert) {
  return check_cert_err_generic(cert,
                                MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE,
                                certificateUsageSSLCA);
}

function check_for_key_type(key_type) {
  // OK CA -> OK INT -> OK EE
  check_ok_ca(load_cert(key_type + "-caOK", "CTu,CTu,CTu"));
  check_ok_ca(load_cert(key_type + "-intOK-caOK", ",,"));
  check_ok(certFromFile(key_type + "-eeOK-intOK-caOK.der"));

  // Bad CA -> OK INT -> OK EE
  check_fail_ca(load_cert(key_type + "-caBad", "CTu,CTu,CTu"));
  check_fail_ca(load_cert(key_type + "-intOK-caBad", ",,"));
  check_fail(certFromFile(key_type + "-eeOK-intOK-caBad.der"));

  // OK CA -> Bad INT -> OK EE
  check_fail_ca(load_cert(key_type + "-intBad-caOK", ",,"));
  check_fail(certFromFile(key_type + "-eeOK-intBad-caOK.der"));

  // OK CA -> OK INT -> Bad EE
  check_fail(certFromFile(key_type + "-eeBad-intOK-caOK.der"));
}

function run_test() {
  check_for_key_type("rsa");
  check_for_key_type("dsa");

  run_next_test();
}
