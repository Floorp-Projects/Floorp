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

function checkForKeyType(keyType, inadequateKeySize, adequateKeySize) {
  let rootOKName = "root_" + keyType + "_" + adequateKeySize;
  let rootNotOKName = "root_" + keyType + "_" + inadequateKeySize;
  let intOKName = "int_" + keyType + "_" + adequateKeySize;
  let intNotOKName = "int_" + keyType + "_" + inadequateKeySize;
  let eeOKName = "ee_" + keyType + "_" + adequateKeySize;
  let eeNotOKName = "ee_" + keyType + "_" + inadequateKeySize;

  // Chain with certs that have adequate sizes for DV
  let intFullName = intOKName + "-" + rootOKName;
  let eeFullName = eeOKName + "-" + intOKName + "-" + rootOKName;
  check_ok_ca(load_cert(rootOKName, "CTu,CTu,CTu"));
  check_ok_ca(load_cert(intFullName, ",,"));
  check_ok(certFromFile(eeFullName + ".der"));

  // Chain with a root cert that has an inadequate size for DV
  intFullName = intOKName + "-" + rootNotOKName;
  eeFullName = eeOKName + "-" + intOKName + "-" + rootNotOKName;
  check_fail_ca(load_cert(rootNotOKName, "CTu,CTu,CTu"));
  check_fail_ca(load_cert(intFullName, ",,"));
  check_fail(certFromFile(eeFullName + ".der"));

  // Chain with an intermediate cert that has an inadequate size for DV
  intFullName = intNotOKName + "-" + rootOKName;
  eeFullName = eeOKName + "-" + intNotOKName + "-" + rootOKName;
  check_fail_ca(load_cert(intFullName, ",,"));
  check_fail(certFromFile(eeFullName + ".der"));

  // Chain with an end entity cert that has an inadequate size for DV
  eeFullName = eeNotOKName + "-" + intOKName + "-" + rootOKName;
  check_fail(certFromFile(eeFullName + ".der"));
}

function run_test() {
  checkForKeyType("rsa", 1016, 1024);
  checkForKeyType("dsa", 960, 1024);

  run_next_test();
}
