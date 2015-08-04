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
 * @param {PRErrorCode} eeExpectedError
 */
function checkChain(rootKeyType, rootKeySize, intKeyType, intKeySize,
                    eeKeyType, eeKeySize, eeExpectedError) {
  let rootName = "root_" + rootKeyType + "_" + rootKeySize;
  let intName = "int_" + intKeyType + "_" + intKeySize;
  let eeName = "ee_" + eeKeyType + "_" + eeKeySize;

  let intFullName = intName + "-" + rootName;
  let eeFullName = eeName + "-" + intName + "-" + rootName;

  addCertFromFile(certdb, `test_keysize/${rootName}.pem`, "CTu,CTu,CTu");
  addCertFromFile(certdb, `test_keysize/${intFullName}.pem`, ",,");
  let eeCert = constructCertFromFile(`test_keysize/${eeFullName}.pem`);

  do_print("cert o=" + eeCert.organization);
  do_print("cert issuer o=" + eeCert.issuerOrganization);
  checkCertErrorGeneric(certdb, eeCert, eeExpectedError,
                        certificateUsageSSLServer);
}

/**
 * Tests various RSA chains.
 *
 * @param {Number} inadequateKeySize
 * @param {Number} adequateKeySize
 */
function checkRSAChains(inadequateKeySize, adequateKeySize) {
  // Chain with certs that have adequate sizes for DV
  checkChain("rsa", adequateKeySize,
             "rsa", adequateKeySize,
             "rsa", adequateKeySize,
             PRErrorCodeSuccess);

  // Chain with a root cert that has an inadequate size for DV
  checkChain("rsa", inadequateKeySize,
             "rsa", adequateKeySize,
             "rsa", adequateKeySize,
             MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE);

  // Chain with an intermediate cert that has an inadequate size for DV
  checkChain("rsa", adequateKeySize,
             "rsa", inadequateKeySize,
             "rsa", adequateKeySize,
             MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE);

  // Chain with an end entity cert that has an inadequate size for DV
  checkChain("rsa", adequateKeySize,
             "rsa", adequateKeySize,
             "rsa", inadequateKeySize,
             MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE);
}

function checkECCChains() {
  checkChain("prime256v1", 256,
             "secp384r1", 384,
             "secp521r1", 521,
             PRErrorCodeSuccess);
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
             PRErrorCodeSuccess);
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
  checkRSAChains(1016, 1024);
  checkECCChains();
  checkCombinationChains();

  run_next_test();
}
