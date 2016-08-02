/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This test attempts to ensure that PSM doesn't deadlock or crash when shutting
// down NSS while a background thread is attempting to use NSS.
// Uses test_signed_apps/valid_app_1.zip from test_signed_apps.js.

function startAsyncNSSOperation(certdb, appFile) {
  return new Promise((resolve, reject) => {
    certdb.openSignedAppFileAsync(Ci.nsIX509CertDB.AppXPCShellRoot, appFile,
      function(rv, aZipReader, aSignerCert) {
        // rv will either indicate success (if NSS hasn't been shut down yet) or
        // it will be some error code that varies depending on when NSS got shut
        // down. As such, there's nothing really to check here. Just resolve the
        // promise to continue execution.
        resolve();
      });
  });
}

add_task(function* () {
  do_get_profile();
  let psm = Cc["@mozilla.org/psm;1"]
              .getService(Ci.nsISupports)
              .QueryInterface(Ci.nsIObserver);
  let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);
  let appFile = do_get_file("test_signed_apps/valid_app_1.zip");

  let promises = [];
  for (let i = 0; i < 25; i++) {
    promises.push(startAsyncNSSOperation(certdb, appFile));
  }
  // Trick PSM into thinking it should shut down NSS. If this test doesn't
  // hang or crash, we're good.
  psm.observe(null, "profile-before-change", null);
  for (let i = 0; i < 25; i++) {
    promises.push(startAsyncNSSOperation(certdb, appFile));
  }
  yield Promise.all(promises);
});
