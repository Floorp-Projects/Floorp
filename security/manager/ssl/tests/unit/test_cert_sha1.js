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
  return checkCertErrorGenericAtTime(certdb, cert, expectedResult,
                                     certificateUsageSSLServer,
                                     VALIDATION_TIME);
}

add_task(async function() {
  loadCertWithTrust("ca", "CTu,,");
  loadCertWithTrust("int-pre", ",,");
  loadCertWithTrust("int-post", ",,");

  // Test cases per pref setting
  //
  // root  intermed.  end entity
  // ===========================
  // root
  //  |
  //  +--- pre-2016
  //  |       |
  //  |       +----- pre-2016   <--- (a)
  //  |       +----- post-2016  <--- (b)
  //  |
  //  +--- post-2016
  //          |
  //          +----- post-2016  <--- (c)
  //
  // Expected outcomes (accept / reject):
  //
  //                              a      b      c
  // Allowed (0)                  Accept Accept Accept
  // Forbidden (1)                Reject Reject Reject
  // (2) is no longer available and is treated as Forbidden (1) internally.
  // ImportedRoot (3)             Reject Reject Reject (for built-in roots)
  // ImportedRoot                 Accept Accept Accept (for non-built-in roots)
  // ImportedRootOrBefore2016 (4) Accept Reject Reject (for built-in roots)
  // ImportedRootOrBefore2016     Accept Accept Accept (for non-built-in roots)
  //
  // The pref setting of ImportedRoot accepts usage of SHA-1 only for
  // certificates issued by non-built-in roots. By default, the testing
  // certificates are all considered issued by a non-built-in root. However, we
  // have the ability to treat a given non-built-in root as built-in. We test
  // both of these situations below.
  //
  // As a historical note, a policy option (Before2016) was previously available
  // that only allowed SHA-1 for certificates with a notBefore before 2016.
  // However, to enable the policy of only allowing SHA-1 from non-built-in
  // roots in the most straightforward way (while still having a time-based
  // policy that users could enable if this new policy were problematic),
  // Before2016 was shifted to also allow SHA-1 from non-built-in roots, hence
  // ImportedRootOrBefore2016.
  //
  // A note about intermediate certificates: the certificate verifier has the
  // ability to directly verify a given certificate for the purpose of issuing
  // TLS web server certificates. However, when asked to do so, the certificate
  // verifier does not take into account the currently configured SHA-1 policy.
  // This is in part due to implementation complexity and because this isn't
  // actually how TLS web server certificates are verified in the TLS handshake
  // (which makes a full implementation that supports heeding the SHA-1 policy
  // unnecessary).

  // SHA-1 allowed
  Services.prefs.setIntPref("security.pki.sha1_enforcement_level", 0);
  await checkEndEntity(certFromFile("ee-pre_int-pre"), PRErrorCodeSuccess);
  await checkEndEntity(certFromFile("ee-post_int-pre"), PRErrorCodeSuccess);
  await checkEndEntity(certFromFile("ee-post_int-post"), PRErrorCodeSuccess);

  // SHA-1 forbidden
  Services.prefs.setIntPref("security.pki.sha1_enforcement_level", 1);
  await checkEndEntity(certFromFile("ee-pre_int-pre"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);
  await checkEndEntity(certFromFile("ee-post_int-pre"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);
  await checkEndEntity(certFromFile("ee-post_int-post"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);

  // SHA-1 forbidden (test the case where the pref has been set to 2)
  Services.prefs.setIntPref("security.pki.sha1_enforcement_level", 2);
  await checkEndEntity(certFromFile("ee-pre_int-pre"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);
  await checkEndEntity(certFromFile("ee-post_int-pre"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);
  await checkEndEntity(certFromFile("ee-post_int-post"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);

  // SHA-1 allowed only when issued by an imported root. First test with the
  // test root considered a built-in (on debug only - this functionality is
  // disabled on non-debug builds).
  Services.prefs.setIntPref("security.pki.sha1_enforcement_level", 3);
  if (isDebugBuild) {
    let root = certFromFile("ca");
    Services.prefs.setCharPref("security.test.built_in_root_hash", root.sha256Fingerprint);
    await checkEndEntity(certFromFile("ee-pre_int-pre"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);
    await checkEndEntity(certFromFile("ee-post_int-pre"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);
    await checkEndEntity(certFromFile("ee-post_int-post"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);
    Services.prefs.clearUserPref("security.test.built_in_root_hash");
  }

  // SHA-1 still allowed only when issued by an imported root.
  // Now test with the test root considered a non-built-in.
  await checkEndEntity(certFromFile("ee-pre_int-pre"), PRErrorCodeSuccess);
  await checkEndEntity(certFromFile("ee-post_int-pre"), PRErrorCodeSuccess);
  await checkEndEntity(certFromFile("ee-post_int-post"), PRErrorCodeSuccess);

  // SHA-1 allowed before 2016 or when issued by an imported root. First test
  // with the test root considered a built-in.
  Services.prefs.setIntPref("security.pki.sha1_enforcement_level", 4);
  if (isDebugBuild) {
    let root = certFromFile("ca");
    Services.prefs.setCharPref("security.test.built_in_root_hash", root.sha256Fingerprint);
    await checkEndEntity(certFromFile("ee-pre_int-pre"), PRErrorCodeSuccess);
    await checkEndEntity(certFromFile("ee-post_int-pre"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);
    await checkEndEntity(certFromFile("ee-post_int-post"), SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED);
    Services.prefs.clearUserPref("security.test.built_in_root_hash");
  }

  // SHA-1 still only allowed before 2016 or when issued by an imported root.
  // Now test with the test root considered a non-built-in.
  await checkEndEntity(certFromFile("ee-pre_int-pre"), PRErrorCodeSuccess);
  await checkEndEntity(certFromFile("ee-post_int-pre"), PRErrorCodeSuccess);
  await checkEndEntity(certFromFile("ee-post_int-post"), PRErrorCodeSuccess);
});
