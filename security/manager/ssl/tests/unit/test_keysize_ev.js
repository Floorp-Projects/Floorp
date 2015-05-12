// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Checks that RSA certs with key sizes below 2048 bits when verifying for EV
// are rejected.

do_get_profile(); // Must be called before getting nsIX509CertDB
const certDB = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

const SERVER_PORT = 8888;

function getOCSPResponder(expectedCertNames) {
  let expectedPaths = expectedCertNames.slice();
  return startOCSPResponder(SERVER_PORT, "www.example.com", [],
                            "test_keysize", expectedCertNames, expectedPaths);
}

function certFromFile(filename) {
  let der = readFile(do_get_file("test_keysize/" + filename, false));
  return certDB.constructX509(der, der.length);
}

function loadCert(certName, trustString) {
  let certFilename = certName + ".der";
  addCertFromFile(certDB, "test_keysize/" + certFilename, trustString);
  return certFromFile(certFilename);
}

function checkEVStatus(cert, usage, isEVExpected) {
  do_print("cert cn=" + cert.commonName);
  do_print("cert o=" + cert.organization);
  do_print("cert issuer cn=" + cert.issuerCommonName);
  do_print("cert issuer o=" + cert.issuerOrganization);
  let hasEVPolicy = {};
  let verifiedChain = {};
  let error = certDB.verifyCertNow(cert, usage, NO_FLAGS, null, verifiedChain,
                                   hasEVPolicy);
  equal(hasEVPolicy.value, isEVExpected);
  equal(error, PRErrorCodeSuccess);
}

/**
 * Adds a single EV key size test.
 *
 * @param {Array} expectedNamesForOCSP
 *        An array of nicknames of the certs to be responded to.
 * @param {String} rootCertFileName
 *        The file name of the root cert. Can begin with ".." to reference
 *        certs in folders other than "test_keysize/".
 * @param {Array} intCertFileNames
 *        An array of file names of any intermediate certificates.
 * @param {String} endEntityCertFileName
 *        The file name of the end entity cert.
 * @param {Boolean} expectedResult
 *        Whether the chain is expected to validate as EV.
 */
function addKeySizeTestForEV(expectedNamesForOCSP,
                             rootCertFileName, intCertFileNames,
                             endEntityCertFileName, expectedResult)
{
  add_test(function() {
    clearOCSPCache();
    let ocspResponder = getOCSPResponder(expectedNamesForOCSP);

    loadCert(rootCertFileName, "CTu,CTu,CTu");
    for (let intCertFileName of intCertFileNames) {
      loadCert(intCertFileName, ",,");
    }
    checkEVStatus(certFromFile(endEntityCertFileName + ".der"),
                  certificateUsageSSLServer, expectedResult);

    ocspResponder.stop(run_next_test);
  });
}

/**
 * For debug builds which have the test EV roots compiled in, checks RSA chains
 * which contain certs with key sizes adequate for EV are validated as such,
 * while chains that contain any cert with an inadequate key size fail EV and
 * validate as DV.
 * For opt builds which don't have the test EV roots compiled in, checks that
 * none of the chains validate as EV.
 *
 * Note: This function assumes that the key size requirements for EV are greater
 * than the requirements for DV.
 *
 * @param {Number} inadequateKeySize
 *        The inadequate key size of the generated certs.
 * @param {Number} adequateKeySize
 *        The adequate key size of the generated certs.
 */
function checkRSAChains(inadequateKeySize, adequateKeySize) {
  // Reuse the existing test RSA EV root
  let rootOKCertFileName = "../test_ev_certs/evroot";
  let rootOKName = "evroot";
  let rootNotOKName = "ev_root_rsa_" + inadequateKeySize;
  let intOKName = "ev_int_rsa_" + adequateKeySize;
  let intNotOKName = "ev_int_rsa_" + inadequateKeySize;
  let eeOKName = "ev_ee_rsa_" + adequateKeySize;
  let eeNotOKName = "ev_ee_rsa_" + inadequateKeySize;

  // Chain with certs that have adequate sizes for EV and DV
  // In opt builds, this chain is only validated for DV. Hence, an OCSP fetch
  // will for example not be done for the "ev_int_rsa_2048-evroot" intermediate
  // in such a build.
  let intFullName = intOKName + "-" + rootOKName;
  let eeFullName = eeOKName + "-" + intOKName + "-" + rootOKName;
  let expectedNamesForOCSP = gEVExpected
                           ? [ intFullName,
                               eeFullName ]
                           : [ eeFullName ];
  addKeySizeTestForEV(expectedNamesForOCSP, rootOKCertFileName,
                      [ intFullName ], eeFullName, gEVExpected);

  // Chain with a root cert that has an inadequate size for EV, but
  // adequate size for DV
  intFullName = intOKName + "-" + rootNotOKName;
  eeFullName = eeOKName + "-" + intOKName + "-" + rootNotOKName;
  expectedNamesForOCSP = [ eeFullName ];
  addKeySizeTestForEV(expectedNamesForOCSP, rootNotOKName,
                      [ intFullName ], eeFullName, false);

  // Chain with an intermediate cert that has an inadequate size for EV, but
  // adequate size for DV
  intFullName = intNotOKName + "-" + rootOKName;
  eeFullName = eeOKName + "-" + intNotOKName + "-" + rootOKName;
  expectedNamesForOCSP = [ eeFullName ];
  addKeySizeTestForEV(expectedNamesForOCSP, rootOKCertFileName,
                      [ intFullName ], eeFullName, false);

  // Chain with an end entity cert that has an inadequate size for EV, but
  // adequate size for DV
  intFullName = intOKName + "-" + rootOKName;
  eeFullName = eeNotOKName + "-" + intOKName + "-" + rootOKName;
  expectedNamesForOCSP = gEVExpected
                       ? [ intFullName,
                           eeFullName ]
                       : [ eeFullName ];
  addKeySizeTestForEV(expectedNamesForOCSP, rootOKCertFileName,
                      [ intFullName ], eeFullName, false);
}

function run_test() {
  Services.prefs.setCharPref("network.dns.localDomains", "www.example.com");

  checkRSAChains(2040, 2048);

  run_next_test();
}
