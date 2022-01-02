// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Tests that starting a profile with a preexisting CRLite filter and stash
// works correctly.

"use strict";

add_task(async function test_preexisting_crlite_data() {
  Services.prefs.setIntPref(
    "security.pki.crlite_mode",
    CRLiteModeEnforcePrefValue
  );

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  // These need to be available to be able to find them during path building
  // for certificate verification.
  let issuerCert = constructCertFromFile("test_crlite_filters/issuer.pem");
  ok(issuerCert, "issuer certificate should decode successfully");
  let noSCTCertIssuer = constructCertFromFile(
    "test_crlite_filters/no-sct-issuer.pem"
  );
  ok(
    noSCTCertIssuer,
    "issuer certificate for certificate without SCTs should decode successfully"
  );

  let validCert = constructCertFromFile("test_crlite_filters/valid.pem");
  // NB: by not specifying Ci.nsIX509CertDB.FLAG_LOCAL_ONLY, this tests that
  // the implementation does not fall back to OCSP fetching, because if it
  // did, the implementation would attempt to connect to a server outside the
  // test infrastructure, which would result in a crash in the test
  // environment, which would be treated as a test failure.
  await checkCertErrorGenericAtTime(
    certdb,
    validCert,
    PRErrorCodeSuccess,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "vpn.worldofspeed.org",
    0
  );

  let revokedCert = constructCertFromFile("test_crlite_filters/revoked.pem");
  await checkCertErrorGenericAtTime(
    certdb,
    revokedCert,
    SEC_ERROR_REVOKED_CERTIFICATE,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "us-datarecovery.com",
    0
  );

  let revokedInStashCert = constructCertFromFile(
    "test_crlite_filters/revoked-in-stash.pem"
  );
  // The stash may not have loaded yet, so await a task that ensures the stash
  // loading task has completed.
  let certStorage = Cc["@mozilla.org/security/certstorage;1"].getService(
    Ci.nsICertStorage
  );
  await new Promise(resolve => {
    certStorage.hasPriorData(Ci.nsICertStorage.DATA_TYPE_CRLITE, (rv, _) => {
      Assert.equal(rv, Cr.NS_OK, "hasPriorData should succeed");
      resolve();
    });
  });
  await checkCertErrorGenericAtTime(
    certdb,
    revokedInStashCert,
    SEC_ERROR_REVOKED_CERTIFICATE,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "stokedmoto.com",
    0
  );

  let revokedInStash2Cert = constructCertFromFile(
    "test_crlite_filters/revoked-in-stash-2.pem"
  );
  await checkCertErrorGenericAtTime(
    certdb,
    revokedInStash2Cert,
    SEC_ERROR_REVOKED_CERTIFICATE,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "icsreps.com",
    0
  );

  // This certificate has no embedded SCTs, so it is not guaranteed to be in
  // CT, so CRLite can't be guaranteed to give the correct answer, so it is
  // not consulted, and the implementation falls back to OCSP. Since the real
  // OCSP responder can't be reached, this results in a
  // SEC_ERROR_OCSP_SERVER_ERROR.
  let noSCTCert = constructCertFromFile("test_crlite_filters/no-sct.pem");
  // NB: this will cause an OCSP request to be sent to localhost:80, but
  // since an OCSP responder shouldn't be running on that port, this should
  // fail safely.
  Services.prefs.setCharPref("network.dns.localDomains", "ocsp.digicert.com");
  Services.prefs.setBoolPref("security.OCSP.require", true);
  Services.prefs.setIntPref("security.OCSP.enabled", 1);
  await checkCertErrorGenericAtTime(
    certdb,
    noSCTCert,
    SEC_ERROR_OCSP_SERVER_ERROR,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "mail233.messagelabs.com",
    0
  );
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.clearUserPref("security.OCSP.require");
  Services.prefs.clearUserPref("security.OCSP.enabled");

  // If the earliest certificate timestamp is within the merge delay of the
  // logs for the filter we have, it won't be looked up, and thus won't be
  // revoked.
  // The earliest timestamp in this certificate is in August 2020, whereas
  // the filter timestamp is in October 2020, so setting the merge delay to
  // this large value simluates the situation being tested.
  Services.prefs.setIntPref(
    "security.pki.crlite_ct_merge_delay_seconds",
    60 * 60 * 24 * 60
  );
  // Since setting the merge delay parameter this way effectively makes this
  // certificate "too new" to be covered by the filter, the implementation
  // would fall back to OCSP fetching. Since this would result in a crash and
  // test failure, the Ci.nsIX509CertDB.FLAG_LOCAL_ONLY is used.
  await checkCertErrorGenericAtTime(
    certdb,
    revokedCert,
    PRErrorCodeSuccess,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    false,
    "us-datarecovery.com",
    Ci.nsIX509CertDB.FLAG_LOCAL_ONLY
  );
  Services.prefs.clearUserPref("security.pki.crlite_ct_merge_delay_seconds");
});

function run_test() {
  let securityStateDirectory = do_get_profile();
  securityStateDirectory.append("security_state");
  // For simplicity, re-use the filter from test_crlite_filters.js.
  let crilteFile = do_get_file("test_crlite_filters/20201017-0-filter");
  crilteFile.copyTo(securityStateDirectory, "crlite.filter");
  // This stash file and the following cert storage file were obtained by
  // running just the task `test_crlite_filters_and_check_revocation` in
  // test_crlite_filters.js, causing it to hang (by adding something like
  // `add_test(() => {});`), and then copying the files from the temporary
  // profile directory.
  let stashFile = do_get_file("test_crlite_preexisting/crlite.stash");
  stashFile.copyTo(securityStateDirectory, "crlite.stash");
  let certStorageFile = do_get_file("test_crlite_preexisting/data.safe.bin");
  certStorageFile.copyTo(securityStateDirectory, "data.safe.bin");

  run_next_test();
}
