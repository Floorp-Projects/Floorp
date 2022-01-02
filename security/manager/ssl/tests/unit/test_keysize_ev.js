// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Checks that RSA certs with key sizes below 2048 bits when verifying for EV
// are rejected.

do_get_profile(); // Must be called before getting nsIX509CertDB
const certDB = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

const SERVER_PORT = 8888;

function getOCSPResponder(expectedCertNames) {
  let expectedPaths = expectedCertNames.slice();
  return startOCSPResponder(
    SERVER_PORT,
    "www.example.com",
    "test_keysize_ev/",
    expectedCertNames,
    expectedPaths
  );
}

function loadCert(certName, trustString) {
  let certFilename = "test_keysize_ev/" + certName + ".pem";
  addCertFromFile(certDB, certFilename, trustString);
  return constructCertFromFile(certFilename);
}

/**
 * Asynchronously runs a single EV key size test.
 *
 * @param {Array} expectedNamesForOCSP
 *        An array of nicknames of the certs to be responded to.
 * @param {String} rootCertFileName
 *        The file name of the root cert. Can begin with ".." to reference
 *        certs in folders other than "test_keysize_ev/".
 * @param {Array} intCertFileNames
 *        An array of file names of any intermediate certificates.
 * @param {String} endEntityCertFileName
 *        The file name of the end entity cert.
 * @param {Boolean} expectedResult
 *        Whether the chain is expected to validate as EV.
 */
async function keySizeTestForEV(
  expectedNamesForOCSP,
  rootCertFileName,
  intCertFileNames,
  endEntityCertFileName,
  expectedResult
) {
  clearOCSPCache();
  let ocspResponder = getOCSPResponder(expectedNamesForOCSP);

  loadCert(rootCertFileName, "CTu,CTu,CTu");
  for (let intCertFileName of intCertFileNames) {
    loadCert(intCertFileName, ",,");
  }
  await checkEVStatus(
    certDB,
    constructCertFromFile(`test_keysize_ev/${endEntityCertFileName}.pem`),
    certificateUsageSSLServer,
    expectedResult
  );

  await stopOCSPResponder(ocspResponder);
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
async function checkRSAChains(inadequateKeySize, adequateKeySize) {
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
    ? [intFullName, eeFullName]
    : [eeFullName];
  await keySizeTestForEV(
    expectedNamesForOCSP,
    rootOKCertFileName,
    [intFullName],
    eeFullName,
    gEVExpected
  );

  // Chain with a root cert that has an inadequate size for EV, but
  // adequate size for DV
  intFullName = intOKName + "-" + rootNotOKName;
  eeFullName = eeOKName + "-" + intOKName + "-" + rootNotOKName;
  expectedNamesForOCSP = [eeFullName];
  await keySizeTestForEV(
    expectedNamesForOCSP,
    rootNotOKName,
    [intFullName],
    eeFullName,
    false
  );

  // Chain with an intermediate cert that has an inadequate size for EV, but
  // adequate size for DV
  intFullName = intNotOKName + "-" + rootOKName;
  eeFullName = eeOKName + "-" + intNotOKName + "-" + rootOKName;
  expectedNamesForOCSP = [eeFullName];
  await keySizeTestForEV(
    expectedNamesForOCSP,
    rootOKCertFileName,
    [intFullName],
    eeFullName,
    false
  );

  // Chain with an end entity cert that has an inadequate size for EV, but
  // adequate size for DV
  intFullName = intOKName + "-" + rootOKName;
  eeFullName = eeNotOKName + "-" + intOKName + "-" + rootOKName;
  expectedNamesForOCSP = gEVExpected ? [intFullName, eeFullName] : [eeFullName];
  await keySizeTestForEV(
    expectedNamesForOCSP,
    rootOKCertFileName,
    [intFullName],
    eeFullName,
    false
  );
}

add_task(async function() {
  Services.prefs.setCharPref("network.dns.localDomains", "www.example.com");
  Services.prefs.setIntPref("security.OCSP.enabled", 1);

  let smallKeyEVRoot = constructCertFromFile(
    "test_keysize_ev/ev_root_rsa_2040.pem"
  );
  equal(
    smallKeyEVRoot.sha256Fingerprint,
    "40:AB:5D:A5:89:15:A9:4B:82:87:B8:A6:9A:84:B1:DB:" +
      "7A:9D:DB:B8:4E:E1:23:E3:C6:64:E7:50:DC:35:8C:68",
    "test sanity check: the small-key EV root must have the same " +
      "fingerprint as the corresponding entry in ExtendedValidation.cpp"
  );

  await checkRSAChains(2040, 2048);
});
