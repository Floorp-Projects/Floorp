// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Tests that CRLite is left in the uninitialized state when the profile
// contains a corrupted stash file.

"use strict";

add_task(async function test_crlite_stash_corrupted() {
  let securityStateDirectory = do_get_profile();
  securityStateDirectory.append("security_state");

  Services.prefs.setIntPref(
    "security.pki.crlite_mode",
    CRLiteModeEnforcePrefValue
  );

  let coverage = do_get_file("test_crlite_preexisting/crlite.coverage");
  coverage.copyTo(securityStateDirectory, "crlite.coverage");

  let enrollment = do_get_file("test_crlite_preexisting/crlite.enrollment");
  enrollment.copyTo(securityStateDirectory, "crlite.enrollment");

  let filter = do_get_file("test_crlite_filters/20201017-0-filter");
  filter.copyTo(securityStateDirectory, "crlite.filter");

  let stash = do_get_file("test_crlite_corrupted/bad.stash");
  stash.copyTo(securityStateDirectory, "crlite.stash");

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );

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

  // Await a task that ensures the stash loading task has completed.
  await new Promise(resolve => {
    certStorage.hasPriorData(
      Ci.nsICertStorage.DATA_TYPE_CRLITE_FILTER_INCREMENTAL,
      (rv, _) => {
        Assert.equal(rv, Cr.NS_OK, "hasPriorData should succeed");
        resolve();
      }
    );
  });

  // This certificate is revoked according to `test_crlite_filters/20201017-0-filter`.
  // Its issuer is enrolled according to `test_crlite_preexisting/crlite.enrollment`,
  // and it is covered according to `test_crlite_preexisting/crlite.coverage`.
  let revokedCert = constructCertFromFile("test_crlite_filters/revoked.pem");

  // The issuer's certificate needs to be available for path building.
  let issuerCert = constructCertFromFile("test_crlite_filters/issuer.pem");
  ok(issuerCert, "issuer certificate should decode successfully");

  // Loading the stash should not have caused any problems, and `revokedCert`
  // should be marked as revoked.
  await checkCertErrorGenericAtTime(
    certdb,
    revokedCert,
    SEC_ERROR_REVOKED_CERTIFICATE,
    certificateUsageSSLServer,
    new Date("2020-10-20T00:00:00Z").getTime() / 1000,
    undefined,
    "us-datarecovery.com",
    0
  );

  let hasFilter = await new Promise(resolve => {
    certStorage.hasPriorData(
      Ci.nsICertStorage.DATA_TYPE_CRLITE_FILTER_FULL,
      (rv, result) => {
        Assert.equal(rv, Cr.NS_OK, "hasPriorData should succeed");
        resolve(result);
      }
    );
  });
  Assert.equal(hasFilter, true, "CRLite should have a filter");
});
