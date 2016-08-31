// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Tests that chains containing an end-entity cert with an overly long validity
// period are rejected.

do_get_profile(); // Must be called before getting nsIX509CertDB
const certDB = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

const SERVER_PORT = 8888;

function getOCSPResponder(expectedCertNames) {
  let expectedPaths = expectedCertNames.slice();
  return startOCSPResponder(SERVER_PORT, "www.example.com", "test_validity",
                            expectedCertNames, expectedPaths);
}

function certFromFile(filename) {
  return constructCertFromFile(`test_validity/${filename}`);
}

function loadCert(certFilename, trustString) {
  addCertFromFile(certDB, `test_validity/${certFilename}`, trustString);
}

/**
 * Adds a single EV test.
 *
 * @param {Array} expectedNamesForOCSP
 *        An array of nicknames of the certs to be responded to.
 * @param {String} rootCertFileName
 *        The file name of the root cert. Can begin with ".." to reference
 *        certs in folders other than "test_validity/".
 * @param {Array} intCertFileNames
 *        An array of file names of any intermediate certificates.
 * @param {String} endEntityCertFileName
 *        The file name of the end entity cert.
 * @param {Boolean} expectedResult
 *        Whether the chain is expected to validate as EV.
 */
function addEVTest(expectedNamesForOCSP, rootCertFileName, intCertFileNames,
                   endEntityCertFileName, expectedResult)
{
  add_test(function() {
    clearOCSPCache();
    let ocspResponder = getOCSPResponder(expectedNamesForOCSP);

    loadCert(`${rootCertFileName}.pem`, "CTu,CTu,CTu");
    for (let intCertFileName of intCertFileNames) {
      loadCert(`${intCertFileName}.pem`, ",,");
    }
    checkEVStatus(certDB, certFromFile(`${endEntityCertFileName}.pem`),
                  certificateUsageSSLServer, expectedResult);

    ocspResponder.stop(run_next_test);
  });
}

function checkEVChains() {
  // Chain with an end entity cert with a validity period that is acceptable
  // for EV.
  const intFullName = "ev_int_60_months-evroot";
  let eeFullName = `ev_ee_27_months-${intFullName}`;
  let expectedNamesForOCSP = gEVExpected
                           ? [ intFullName,
                               eeFullName ]
                           : [ eeFullName ];
  addEVTest(expectedNamesForOCSP, "../test_ev_certs/evroot", [ intFullName ],
            eeFullName, gEVExpected);

  // Chain with an end entity cert with a validity period that is too long
  // for EV.
  eeFullName = `ev_ee_28_months-${intFullName}`;
  expectedNamesForOCSP = gEVExpected
                           ? [ intFullName,
                               eeFullName ]
                           : [ eeFullName ];
  addEVTest(expectedNamesForOCSP, "../test_ev_certs/evroot", [ intFullName ],
            eeFullName, false);
}

function run_test() {
  Services.prefs.setCharPref("network.dns.localDomains", "www.example.com");
  Services.prefs.setIntPref("security.OCSP.enabled", 1);

  checkEVChains();

  run_next_test();
}
