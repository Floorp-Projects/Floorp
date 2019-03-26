// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// Tests that if a server does not send a complete certificate chain, we can
// make use of cached intermediates to build a trust path.

const {TestUtils} = ChromeUtils.import("resource://testing-common/TestUtils.jsm");

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb  = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);

function run_certutil_on_directory(directory, args) {
  let envSvc = Cc["@mozilla.org/process/environment;1"]
                 .getService(Ci.nsIEnvironment);
  let greBinDir = Services.dirsvc.get("GreBinD", Ci.nsIFile);
  envSvc.set("DYLD_LIBRARY_PATH", greBinDir.path);
  // TODO(bug 1107794): Android libraries are in /data/local/xpcb, but "GreBinD"
  // does not return this path on Android, so hard code it here.
  envSvc.set("LD_LIBRARY_PATH", greBinDir.path + ":/data/local/xpcb");
  let certutilBin = _getBinaryUtil("certutil");
  let process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
  process.init(certutilBin);
  args.push("-d");
  args.push(`sql:${directory}`);
  process.run(true, args, args.length);
  Assert.equal(process.exitValue, 0, "certutil should succeed");
}

registerCleanupFunction(() => {
  let certDir = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
  certDir.append("bad_certs");
  Assert.ok(certDir.exists(), "bad_certs should exist");
  let args = [ "-D", "-n", "manually-added-missing-intermediate" ];
  run_certutil_on_directory(certDir.path, args);
});

function run_test() {
  add_tls_server_setup("BadCertServer", "bad_certs");
  // If we don't know about the intermediate, we'll get an unknown issuer error.
  add_connection_test("ee-from-missing-intermediate.example.com",
                      SEC_ERROR_UNKNOWN_ISSUER);

  // Make BadCertServer aware of the intermediate.
  add_test(() => {
    // NB: missing-intermediate.der won't be regenerated when
    // missing-intermediate.pem is. Hopefully by that time we can just use
    // missing-intermediate.pem directly.
    let args = [ "-A", "-n", "manually-added-missing-intermediate", "-i",
                 "test_missing_intermediate/missing-intermediate.der", "-t",
                 ",," ];
    let certDir = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
    certDir.append("bad_certs");
    Assert.ok(certDir.exists(), "bad_certs should exist");
    run_certutil_on_directory(certDir.path, args);
    run_next_test();
  });

  // We have to start observing the topic before there's a chance it gets
  // emitted.
  add_test(() => {
    TestUtils.topicObserved("psm:intermediate-certs-cached").then(run_next_test);
    run_next_test();
  });
  // Connect and cache the intermediate.
  add_connection_test("ee-from-missing-intermediate.example.com",
                      PRErrorCodeSuccess);

  // Add a dummy test so that the only way we advance from here is by observing
  // "psm:intermediate-certs-cached".
  add_test(() => {});

  // Confirm that we cached the intermediate by running a certutil command that
  // will fail if we didn't.
  add_test(() => {
    // Here we have to use the name that gecko chooses to give it.
    let args = [ "-D", "-n", "Missing Intermediate" ];
    run_certutil_on_directory(do_get_profile().path, args);
    run_next_test();
  });

  // Since we cached the intermediate, this should succeed.
  add_connection_test("ee-from-missing-intermediate.example.com",
                      PRErrorCodeSuccess);

  run_next_test();
}
