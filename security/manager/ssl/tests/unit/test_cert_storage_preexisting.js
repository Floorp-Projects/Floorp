/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";


// This file tests that cert_storage correctly persists its "has prior data"
// information across runs of the browser.
// (The test DB files for this test were created by running the test
// `test_cert_storage_broken_db.js` and copying them from that test's profile
// directory.)

add_task({
    skip_if: () => !AppConstants.MOZ_NEW_CERT_STORAGE,
  }, async function() {
  let dbDirectory = do_get_profile();
  dbDirectory.append("security_state");
  let dbFile = do_get_file("test_cert_storage_preexisting/data.mdb");
  dbFile.copyTo(dbDirectory, "data.mdb");
  let lockFile = do_get_file("test_cert_storage_preexisting/lock.mdb");
  lockFile.copyTo(dbDirectory, "lock.mdb");

  let certStorage = Cc["@mozilla.org/security/certstorage;1"].getService(Ci.nsICertStorage);
  let hasPriorRevocationData  = await new Promise((resolve) => {
    certStorage.hasPriorData(Ci.nsICertStorage.DATA_TYPE_REVOCATION, (rv, hasPriorData) => {
      Assert.equal(rv, Cr.NS_OK, "hasPriorData should succeed");
      resolve(hasPriorData);
    });
  });
  Assert.equal(hasPriorRevocationData, true, "should have prior revocation data");

  let hasPriorCertData  = await new Promise((resolve) => {
    certStorage.hasPriorData(Ci.nsICertStorage.DATA_TYPE_CERTIFICATE, (rv, hasPriorData) => {
      Assert.equal(rv, Cr.NS_OK, "hasPriorData should succeed");
      resolve(hasPriorData);
    });
  });
  Assert.equal(hasPriorCertData, true, "should have prior cert data");
});
