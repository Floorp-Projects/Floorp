/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This file tests that cert_storage correctly persists its information across
// runs of the browser specifically in the case of CRLite.
// (The test DB files for this test were created by running the test
// `test_cert_storage_direct.js` and copying them from that test's profile
// directory.)

/* eslint-disable no-unused-vars */
add_task(async function () {
  Services.prefs.setIntPref(
    "security.pki.crlite_mode",
    CRLiteModeEnforcePrefValue
  );

  let dbDirectory = do_get_profile();
  dbDirectory.append("security_state");
  let crliteFile = do_get_file(
    "test_cert_storage_preexisting_crlite/crlite.filter"
  );
  crliteFile.copyTo(dbDirectory, "crlite.filter");
  let coverageFile = do_get_file(
    "test_cert_storage_preexisting_crlite/crlite.coverage"
  );
  coverageFile.copyTo(dbDirectory, "crlite.coverage");
  let enrollmentFile = do_get_file(
    "test_cert_storage_preexisting_crlite/crlite.enrollment"
  );
  enrollmentFile.copyTo(dbDirectory, "crlite.enrollment");

  let certStorage = Cc["@mozilla.org/security/certstorage;1"].getService(
    Ci.nsICertStorage
  );

  // Add an empty stash to ensure the filter is considered to be fresh.
  await new Promise(resolve => {
    certStorage.addCRLiteStash(new Uint8Array([]), (rv, _) => {
      Assert.equal(rv, Cr.NS_OK, "marked filter as fresh");
      resolve();
    });
  });

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  let validCertIssuer = constructCertFromFile(
    "test_cert_storage_direct/valid-cert-issuer.pem"
  );
  let validCert = constructCertFromFile(
    "test_cert_storage_direct/valid-cert.pem"
  );
  await checkCertErrorGenericAtTime(
    certdb,
    validCert,
    PRErrorCodeSuccess,
    certificateUsageSSLServer,
    new Date("2019-10-28T00:00:00Z").getTime() / 1000,
    false,
    "skynew.jp",
    Ci.nsIX509CertDB.FLAG_LOCAL_ONLY
  );

  let revokedCertIssuer = constructCertFromFile(
    "test_cert_storage_direct/revoked-cert-issuer.pem"
  );
  let revokedCert = constructCertFromFile(
    "test_cert_storage_direct/revoked-cert.pem"
  );
  await checkCertErrorGenericAtTime(
    certdb,
    revokedCert,
    SEC_ERROR_REVOKED_CERTIFICATE,
    certificateUsageSSLServer,
    new Date("2019-11-04T00:00:00Z").getTime() / 1000,
    false,
    "schunk-group.com",
    Ci.nsIX509CertDB.FLAG_LOCAL_ONLY
  );
});
