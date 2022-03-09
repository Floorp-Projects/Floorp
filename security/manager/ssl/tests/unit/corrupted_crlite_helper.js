// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Helper file for tests that initialize CRLite with corrupted `security_state`
// files.
//
// Usage:
//   Define nsILocalFile variables for the `crlite.filter`, `crlite.coverage`,
//   and `crlite.enrollment` files that should be copied to the new profile, and
//   then load this file. The variables should be called `filter`, `coverage`,
//   and `enrollment`, respectively. To omit a file, leave the corresponding
//   variable `undefined`.
//
// Example:
//   let filter = do_get_file("some_test_dir/crlite.filter");
//   let coverage = undefined;
//   let enrollment = do_get_file("some_test_dir/crlite.enrollment");
//   load("./corrupted_crlite_helper.js");
//
// Note:
//   The cert_storage library only attempts to read security_state once. So
//   this task can only be included once per test file.

"use strict";

/* eslint-disable no-undef */

add_task(async function test_crlite_corrupted() {
  let securityStateDirectory = do_get_profile();
  securityStateDirectory.append("security_state");

  Services.prefs.setIntPref(
    "security.pki.crlite_mode",
    CRLiteModeEnforcePrefValue
  );

  if (coverage != undefined) {
    coverage.copyTo(securityStateDirectory, "crlite.coverage");
  }
  if (enrollment != undefined) {
    enrollment.copyTo(securityStateDirectory, "crlite.enrollment");
  }
  if (filter != undefined) {
    filter.copyTo(securityStateDirectory, "crlite.filter");
  }

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );

  let certStorage = Cc["@mozilla.org/security/certstorage;1"].getService(
    Ci.nsICertStorage
  );

  // This certificate is revoked according to `test_crlite_filters/20201017-0-filter`.
  // Its issuer is enrolled according to `test_crlite_preexisting/crlite.enrollment`,
  // and it is covered according to `test_crlite_preexisting/crlite.coverage`.
  let revokedCert = constructCertFromFile("test_crlite_filters/revoked.pem");

  // The issuer's certificate needs to be available for path building.
  let issuerCert = constructCertFromFile("test_crlite_filters/issuer.pem");
  ok(issuerCert, "issuer certificate should decode successfully");

  // If we copied a corrupted file to security_state, then CRLite should not be
  // initialized, and we should fall back to OCSP. By setting
  // Ci.nsIX509CertDB.FLAG_LOCAL_ONLY here we skip the OCSP test, so there's no
  // revocation checking, and the revoked certificate should pass inspection.
  await checkCertErrorGenericAtTime(
    certdb,
    revokedCert,
    PRErrorCodeSuccess,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    undefined,
    "us-datarecovery.com",
    Ci.nsIX509CertDB.FLAG_LOCAL_ONLY
  );

  // We should not have a filter or a stash.
  let hasFilter = await new Promise(resolve => {
    certStorage.hasPriorData(
      Ci.nsICertStorage.DATA_TYPE_CRLITE_FILTER_FULL,
      (rv, result) => {
        Assert.equal(rv, Cr.NS_OK, "hasPriorData should succeed");
        resolve(result);
      }
    );
  });
  Assert.equal(hasFilter, false, "CRLite should not have a filter");

  let hasStash = await new Promise(resolve => {
    certStorage.hasPriorData(
      Ci.nsICertStorage.DATA_TYPE_CRLITE_FILTER_INCREMENTAL,
      (rv, result) => {
        Assert.equal(rv, Cr.NS_OK, "hasPriorData should succeed");
        resolve(result);
      }
    );
  });
  Assert.equal(hasStash, false, "CRLite should not have a stash");
});
