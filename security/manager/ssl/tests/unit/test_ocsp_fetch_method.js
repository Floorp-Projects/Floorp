// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// In which we try to validate several ocsp responses, checking in particular
// that we use the specified method for fetching ocsp. We also check what
// POST fallback when an invalid GET response is received.

do_get_profile(); // must be called before getting nsIX509CertDB
const certdb = Cc["@mozilla.org/security/x509certdb;1"]
                 .getService(Ci.nsIX509CertDB);

const SERVER_PORT = 8080;

function start_ocsp_responder(expectedCertNames, expectedPaths,
                              expectedMethods) {
  return startOCSPResponder(SERVER_PORT, "www.example.com", [],
                            "test_ocsp_fetch_method", expectedCertNames,
                            expectedPaths, expectedMethods);
}

function check_cert_err(cert_name, expected_error) {
  let cert = constructCertFromFile("test_ocsp_fetch_method/" + cert_name + ".der");
  return checkCertErrorGeneric(certdb, cert, expected_error,
                               certificateUsageSSLServer);
}

function run_test() {
  addCertFromFile(certdb, "test_ocsp_fetch_method/ca.der", 'CTu,CTu,CTu');
  addCertFromFile(certdb, "test_ocsp_fetch_method/int.der", ',,');

  // Enabled so that we can force ocsp failure responses.
  Services.prefs.setBoolPref("security.OCSP.require", true);

  Services.prefs.setCharPref("network.dns.localDomains",
                             "www.example.com");

  add_test(function() {
    clearOCSPCache();
    Services.prefs.setBoolPref("security.OCSP.GET.enabled", false);
    let ocspResponder = start_ocsp_responder(["a"], [], ["POST"]);
    check_cert_err("a", 0);
    ocspResponder.stop(run_next_test);
  });

  add_test(function() {
    clearOCSPCache();
    Services.prefs.setBoolPref("security.OCSP.GET.enabled", true);
    let ocspResponder = start_ocsp_responder(["a"], [], ["GET"]);
    check_cert_err("a", 0);
    ocspResponder.stop(run_next_test);
  });

  // GET does fallback on bad entry
  add_test(function() {
    clearOCSPCache();
    Services.prefs.setBoolPref("security.OCSP.GET.enabled", true);
    // Bug 1016681 mozilla::pkix does not support fallback yet.
    // let ocspResponder = start_ocsp_responder(["b", "a"], [], ["GET", "POST"]);
    // check_cert_err("a", 0);
    // ocspResponder.stop(run_next_test);
    run_next_test();
  });

  run_next_test();
}
