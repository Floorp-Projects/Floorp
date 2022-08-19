// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// Tests that if a server does not send a complete certificate chain, we can
// make use of cached intermediates to build a trust path.

const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

do_get_profile(); // must be called before getting nsIX509CertDB

registerCleanupFunction(() => {
  let certDir = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
  certDir.append("bad_certs");
  Assert.ok(certDir.exists(), "bad_certs should exist");
  let args = ["-D", "-n", "manually-added-missing-intermediate"];
  run_certutil_on_directory(certDir.path, args, false);
});

function run_test() {
  add_tls_server_setup("BadCertAndPinningServer", "bad_certs");
  // If we don't know about the intermediate, we'll get an unknown issuer error.
  add_connection_test(
    "ee-from-missing-intermediate.example.com",
    SEC_ERROR_UNKNOWN_ISSUER
  );

  // Make BadCertAndPinningServer aware of the intermediate.
  add_test(() => {
    let args = [
      "-A",
      "-n",
      "manually-added-missing-intermediate",
      "-i",
      "test_missing_intermediate/missing-intermediate.pem",
      "-a",
      "-t",
      ",,",
    ];
    let certDir = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
    certDir.append("bad_certs");
    Assert.ok(certDir.exists(), "bad_certs should exist");
    run_certutil_on_directory(certDir.path, args);
    run_next_test();
  });

  // We have to start observing the topic before there's a chance it gets
  // emitted.
  add_test(() => {
    TestUtils.topicObserved("psm:intermediate-certs-cached").then(
      subjectAndData => {
        Assert.equal(subjectAndData.length, 2, "expecting [subject, data]");
        Assert.equal(subjectAndData[1], "1", `expecting "1" cert imported`);
        run_next_test();
      }
    );
    run_next_test();
  });
  // Connect and cache the intermediate.
  add_connection_test(
    "ee-from-missing-intermediate.example.com",
    PRErrorCodeSuccess
  );

  // Add a dummy test so that the only way we advance from here is by observing
  // "psm:intermediate-certs-cached".
  add_test(() => {});

  // Delete the intermediate on the server again.
  add_test(() => {
    clearSessionCache();
    let certDir = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
    certDir.append("bad_certs");
    Assert.ok(certDir.exists(), "bad_certs should exist");
    let args = ["-D", "-n", "manually-added-missing-intermediate"];
    run_certutil_on_directory(certDir.path, args);
    run_next_test();
  });

  // Since we cached the intermediate in gecko, this should succeed.
  add_connection_test(
    "ee-from-missing-intermediate.example.com",
    PRErrorCodeSuccess
  );

  run_next_test();
}
