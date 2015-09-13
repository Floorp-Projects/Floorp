// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Tests the rejection of SHA-1 certificates based on the preference
// security.pki.sha1_enforcement_level.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

// We need to test as if we are at a fixed time, so that we are testing the
// 2016 checks on SHA-1, not the notBefore checks.
//
// (new Date("2016-03-01")).getTime() / 1000
const VALIDATION_TIME = 1456790400;

function certFromFile(certName) {
  return constructCertFromFile("test_cert_sha1/" + certName + ".pem");
}

function loadCertWithTrust(certName, trustString) {
  addCertFromFile(certdb, "test_cert_sha1/" + certName + ".pem", trustString);
}

function checkEndEntity(cert, expectedResult) {
  checkCertErrorGenericAtTime(certdb, cert, expectedResult,
                              certificateUsageSSLServer, VALIDATION_TIME);
}

function checkIntermediate(cert, expectedResult) {
  checkCertErrorGenericAtTime(certdb, cert, expectedResult,
                              certificateUsageSSLCA, VALIDATION_TIME);
}

function run_test() {
  loadCertWithTrust("ca", "CTu,,");
  loadCertWithTrust("int-pre", ",,");
  loadCertWithTrust("int-post", ",,");

  // Test cases per pref setting
  //
  // root  intermed.  end entity
  // ===========================
  // root
  //  |
  //  +--- pre-2016             <--- (a)
  //  |       |
  //  |       +----- pre-2016   <--- (b)
  //  |       +----- post-2016  <--- (c)
  //  |
  //  +--- post-2016            <--- (d)
  //          |
  //          +----- post-2016  <--- (e)
  //
  // Expected outcomes (accept / reject):
  //
  //                     a   b   c   d   e
  // allowed=0          Acc Acc Acc Acc Acc
  // forbidden=1        Rej Rej Rej Rej Rej
  // onlyBefore2016=2   Acc Acc Rej Rej Rej

  // SHA-1 allowed
  Services.prefs.setIntPref("security.pki.sha1_enforcement_level", 0);
  checkIntermediate(certFromFile("int-pre"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee-pre_int-pre"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee-post_int-pre"), PRErrorCodeSuccess);
  checkIntermediate(certFromFile("int-post"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee-post_int-post"), PRErrorCodeSuccess);

  // SHA-1 forbidden
  Services.prefs.setIntPref("security.pki.sha1_enforcement_level", 1);
  checkIntermediate(certFromFile("int-pre"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);
  checkEndEntity(certFromFile("ee-pre_int-pre"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);
  checkEndEntity(certFromFile("ee-post_int-pre"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);
  checkIntermediate(certFromFile("int-post"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);
  checkEndEntity(certFromFile("ee-post_int-post"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);

  // SHA-1 allowed only before 2016
  Services.prefs.setIntPref("security.pki.sha1_enforcement_level", 2);
  checkIntermediate(certFromFile("int-pre"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee-pre_int-pre"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee-post_int-pre"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);
  checkIntermediate(certFromFile("int-post"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);
  checkEndEntity(certFromFile("ee-post_int-post"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);
}
