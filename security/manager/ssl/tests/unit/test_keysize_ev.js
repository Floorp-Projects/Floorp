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
  let error = certDB.verifyCertNow(cert, usage, NO_FLAGS, verifiedChain,
                                   hasEVPolicy);
  equal(hasEVPolicy.value, isEVExpected);
  equal(0, error);
}

/**
 * Adds a single EV key size test.
 *
 * @param {Array} expectedNamesForOCSP
 *        An array of nicknames of the certs to be responded to. The cert name
 *        prefix is not added to the nicknames in this array.
 * @param {String} certNamePrefix
 *        The prefix to prepend to the passed in cert names.
 * @param {String} rootCACertFileName
 *        The file name of the root CA cert. Can begin with ".." to reference
 *        certs in folders other than "test_keysize/".
 * @param {Array} subCACertFileNames
 *        An array of file names of any sub CA certificates.
 * @param {String} endEntityCertFileName
 *        The file name of the end entity cert.
 * @param {Boolean} expectedResult
 *        Whether the chain is expected to validate as EV.
 */
function addKeySizeTestForEV(expectedNamesForOCSP, certNamePrefix,
                             rootCACertFileName, subCACertFileNames,
                             endEntityCertFileName, expectedResult)
{
  add_test(function() {
    clearOCSPCache();
    let ocspResponder = getOCSPResponder(expectedNamesForOCSP);

    // Don't prepend the cert name prefix if rootCACertFileName starts with ".."
    // to support reusing certs in other directories.
    let rootCertNamePrefix = rootCACertFileName.startsWith("..")
                           ? ""
                           : certNamePrefix;
    loadCert(rootCertNamePrefix + rootCACertFileName, "CTu,CTu,CTu");
    for (let subCACertFileName of subCACertFileNames) {
      loadCert(certNamePrefix + subCACertFileName, ",,");
    }
    checkEVStatus(certFromFile(certNamePrefix + endEntityCertFileName + ".der"),
                  certificateUsageSSLServer, expectedResult);

    ocspResponder.stop(run_next_test);
  });
}

/**
 * For debug builds which have the test EV roots compiled in, checks for the
 * given key type that good chains validate as EV, while bad chains fail EV and
 * validate as DV.
 * For opt builds which don't have the test EV roots compiled in, checks that
 * none of the chains validate as EV.
 *
 * Note: This function assumes that the key size requirements for EV are greater
 * than or equal to the requirements for DV.
 *
 * @param {String} keyType
 *        The key type to check (e.g. "rsa").
 */
function checkForKeyType(keyType) {
  let certNamePrefix = "ev-" + keyType;

  // Reuse the existing test RSA EV root
  let rootCAOKCertFileName = keyType == "rsa" ? "../test_ev_certs/evroot"
                                              : "-caOK";

  // OK CA -> OK INT -> OK EE
  // In opt builds, this chain is only validated for DV. Hence, an OCSP fetch
  // will not be done for the "-intOK-caOK" intermediate in such a build.
  let expectedNamesForOCSP = isDebugBuild
                           ? [ certNamePrefix + "-intOK-caOK",
                               certNamePrefix + "-eeOK-intOK-caOK" ]
                           : [ certNamePrefix + "-eeOK-intOK-caOK" ];
  addKeySizeTestForEV(expectedNamesForOCSP, certNamePrefix,
                      rootCAOKCertFileName,
                      ["-intOK-caOK"],
                      "-eeOK-intOK-caOK",
                      isDebugBuild);

  // Bad CA -> OK INT -> OK EE
  expectedNamesForOCSP = [ certNamePrefix + "-eeOK-intOK-caBad" ];
  addKeySizeTestForEV(expectedNamesForOCSP, certNamePrefix,
                      "-caBad",
                      ["-intOK-caBad"],
                      "-eeOK-intOK-caBad",
                      false);

  // OK CA -> Bad INT -> OK EE
  expectedNamesForOCSP = isDebugBuild
                       ? [ certNamePrefix + "-intBad-caOK" ]
                       : [ certNamePrefix + "-eeOK-intBad-caOK" ];
  addKeySizeTestForEV(expectedNamesForOCSP, certNamePrefix,
                      rootCAOKCertFileName,
                      ["-intBad-caOK"],
                      "-eeOK-intBad-caOK",
                      false);

  // OK CA -> OK INT -> Bad EE
  expectedNamesForOCSP = [ certNamePrefix + "-eeBad-intOK-caOK" ];
  addKeySizeTestForEV(expectedNamesForOCSP, certNamePrefix,
                      rootCAOKCertFileName,
                      ["-intOK-caOK"],
                      "-eeBad-intOK-caOK",
                      false);
}

function run_test() {
  // Setup OCSP responder
  Services.prefs.setCharPref("network.dns.localDomains", "www.example.com");

  checkForKeyType("rsa");

  run_next_test();
}
