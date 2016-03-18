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
  // Allowed=0          Acc Acc Acc Acc Acc
  // Forbidden=1        Rej Rej Rej Rej Rej
  // Before2016=2       Acc Acc Rej Rej Rej
  //
  // The pref setting of ImportedRoot (3) accepts usage of SHA-1 for
  // certificates valid before 2016 issued by built-in roots or SHA-1 for
  // certificates issued any time by non-built-in roots. By default, the testing
  // certificates are all considered issued by a non-built-in root. However, we
  // now have the ability to treat a given non-built-in root as built-in. We
  // test both of these situations below.

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

  // SHA-1 allowed only before 2016 or when issued by an imported root. First
  // test with the test root considered a built-in (on debug only - this
  // functionality is disabled on non-debug builds).
  Services.prefs.setIntPref("security.pki.sha1_enforcement_level", 3);
  if (isDebugBuild) {
    let root = certFromFile("ca");
    Services.prefs.setCharPref("security.test.built_in_root_hash", root.sha256Fingerprint);
    checkIntermediate(certFromFile("int-pre"), PRErrorCodeSuccess);
    checkEndEntity(certFromFile("ee-pre_int-pre"), PRErrorCodeSuccess);
    checkEndEntity(certFromFile("ee-post_int-pre"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);
    // This should fail but it doesn't, because the implementation makes no
    // effort to enforce that when verifying a certificate for the capability
    // of issuing TLS server auth certificates (i.e. the
    // "certificateUsageSSLCA" usage), if SHA-1 was necessary, then the root of
    // trust is an imported certificate. We don't really care, though, because
    // the platform doesn't actually make trust decisions in this way and the
    // ability to even verify a certificate for this purpose is intended to go
    // away in bug 1257362.
    // checkIntermediate(certFromFile("int-post"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);
    checkEndEntity(certFromFile("ee-post_int-post"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);
    Services.prefs.clearUserPref("security.test.built_in_root_hash");
  }

  // SHA-1 still allowed only before 2016 or when issued by an imported root.
  // Now test with the test root considered a non-built-in.
  checkIntermediate(certFromFile("int-pre"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee-pre_int-pre"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee-post_int-pre"), PRErrorCodeSuccess);
  checkIntermediate(certFromFile("int-post"), PRErrorCodeSuccess);
  checkEndEntity(certFromFile("ee-post_int-post"), PRErrorCodeSuccess);
}
