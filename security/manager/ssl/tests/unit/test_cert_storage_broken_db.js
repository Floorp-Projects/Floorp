/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";


// This file tests cert_storage's automatic database recreation mechanism. If
// opening the database for the first time fails, cert_storage will re-create
// it.

function call_has_prior_data(certStorage, type) {
  return new Promise((resolve) => {
    certStorage.hasPriorData(type, (rv, hasPriorData) => {
      Assert.equal(rv, Cr.NS_OK, "hasPriorData should succeed");
      resolve(hasPriorData);
    });
  });
}

async function check_has_prior_revocation_data(certStorage, expectedResult) {
  let hasPriorRevocationData = await call_has_prior_data(certStorage,
                                                         Ci.nsICertStorage.DATA_TYPE_REVOCATION);
  Assert.equal(hasPriorRevocationData, expectedResult,
               `should ${expectedResult ? "have" : "not have"} prior revocation data`);
}

async function check_has_prior_cert_data(certStorage, expectedResult) {
  let hasPriorCertData = await call_has_prior_data(certStorage,
                                                   Ci.nsICertStorage.DATA_TYPE_CERTIFICATE);
  Assert.equal(hasPriorCertData, expectedResult,
               `should ${expectedResult ? "have" : "not have"} prior cert data`);
}

add_task({
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  }, async function() {
  // Create an invalid database.
  let fileToCopy = do_get_file("test_cert_storage_broken_db.js");
  let dbDirectory = do_get_profile();
  dbDirectory.append("security_state");
  fileToCopy.copyTo(dbDirectory, "data.mdb");

  let certStorage = Cc["@mozilla.org/security/certstorage;1"].getService(Ci.nsICertStorage);
  check_has_prior_revocation_data(certStorage, false);
  check_has_prior_cert_data(certStorage, false);

  let result = await new Promise((resolve) => {
    certStorage.setRevocations([], resolve);
  });
  Assert.equal(result, Cr.NS_OK, "setRevocations should succeed");

  check_has_prior_revocation_data(certStorage, true);
  check_has_prior_cert_data(certStorage, false);

  result = await new Promise((resolve) => {
    certStorage.addCerts([], resolve);
  });
  Assert.equal(result, Cr.NS_OK, "addCerts should succeed");

  check_has_prior_revocation_data(certStorage, true);
  check_has_prior_cert_data(certStorage, true);
});
