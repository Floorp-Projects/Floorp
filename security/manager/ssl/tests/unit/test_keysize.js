// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

// Checks that RSA certs with key sizes below 1024 bits are rejected.
// Checks that ECC certs using curves other than the NIST P-256, P-384 or P-521
// curves are rejected.

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

/**
 * Tests a cert chain.
 *
 * @param {String} rootKeyType
 *        The key type of the root certificate, or the name of an elliptic
 *        curve, as output by the 'openssl ecparam -list_curves' command.
 * @param {Number} rootKeySize
 * @param {String} intKeyType
 * @param {Number} intKeySize
 * @param {String} eeKeyType
 * @param {Number} eeKeySize
 * @param {Number} eeExpectedError
 */
function checkChain(rootKeyType, rootKeySize, intKeyType, intKeySize,
                    eeKeyType, eeKeySize, eeExpectedError) {
  let rootName = "root_" + rootKeyType + "_" + rootKeySize;
  let intName = "int_" + intKeyType + "_" + intKeySize;
  let eeName = "ee_" + eeKeyType + "_" + eeKeySize;

  let intFullName = intName + "-" + rootName;
  let eeFullName = eeName + "-" + intName + "-" + rootName;

  load_cert(rootName, "CTu,CTu,CTu");
  load_cert(intFullName, ",,");
  let eeCert = certFromFile(eeFullName + ".der")

  do_print("cert cn=" + eeCert.commonName);
  do_print("cert o=" + eeCert.organization);
  do_print("cert issuer cn=" + eeCert.issuerCommonName);
  do_print("cert issuer o=" + eeCert.issuerOrganization);
  checkCertErrorGeneric(certdb, eeCert, eeExpectedError,
                        certificateUsageSSLServer);
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

function checkECCChains() {
  checkChain("prime256v1", 256,
             "secp384r1", 384,
             "secp521r1", 521,
             0);
  checkChain("prime256v1", 256,
             "secp224r1", 224,
             "prime256v1", 256,
             SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE);
  checkChain("prime256v1", 256,
             "prime256v1", 256,
             "secp224r1", 224,
             SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE);
  checkChain("secp224r1", 224,
             "prime256v1", 256,
             "prime256v1", 256,
             SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE);
  checkChain("prime256v1", 256,
             "prime256v1", 256,
             "secp256k1", 256,
             SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE);
  checkChain("secp256k1", 256,
             "prime256v1", 256,
             "prime256v1", 256,
             SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE);
}

function checkCombinationChains() {
  checkChain("rsa", 2048,
             "prime256v1", 256,
             "secp384r1", 384,
             0);
  checkChain("rsa", 2048,
             "prime256v1", 256,
             "secp224r1", 224,
             SEC_ERROR_UNSUPPORTED_ELLIPTIC_CURVE);
  checkChain("prime256v1", 256,
             "rsa", 1016,
             "prime256v1", 256,
             MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE);
}

function run_test() {
  checkForKeyType("rsa", 1016, 1024);
  checkECCChains();
  checkCombinationChains();

  run_next_test();
}
