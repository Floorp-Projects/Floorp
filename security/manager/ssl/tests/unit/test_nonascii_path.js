// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// Tests to make sure that the certificate DB works with non-ASCII paths.

// Append a single quote and non-ASCII characters to the profile path.
let env = Cc["@mozilla.org/process/environment;1"].getService(
  Ci.nsIEnvironment
);
let profd = env.get("XPCSHELL_TEST_PROFILE_DIR");
let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
file.initWithPath(profd);
file.append("'÷1");
env.set("XPCSHELL_TEST_PROFILE_DIR", file.path);

file = do_get_profile(); // must be called before getting nsIX509CertDB
Assert.ok(
  /[^\x20-\x7f]/.test(file.path),
  "the profile path should contain a non-ASCII character"
);
if (mozinfo.os == "win") {
  file.QueryInterface(Ci.nsILocalFileWin);
  Assert.ok(
    /[^\x20-\x7f]/.test(file.canonicalPath),
    "the profile short path should contain a non-ASCII character"
  );
}

// Restore the original value.
env.set("XPCSHELL_TEST_PROFILE_DIR", profd);

const certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
  Ci.nsIX509CertDB
);

function load_cert(cert_name, trust_string) {
  let cert_filename = cert_name + ".pem";
  return addCertFromFile(
    certdb,
    "test_cert_trust/" + cert_filename,
    trust_string
  );
}

function run_test() {
  let certList = ["ca", "int", "ee"];
  let loadedCerts = [];
  for (let certName of certList) {
    loadedCerts.push(load_cert(certName, ",,"));
  }

  let ca_cert = loadedCerts[0];
  notEqual(ca_cert, null, "CA cert should have successfully loaded");
  let int_cert = loadedCerts[1];
  notEqual(int_cert, null, "Intermediate cert should have successfully loaded");
  let ee_cert = loadedCerts[2];
  notEqual(ee_cert, null, "EE cert should have successfully loaded");
}
