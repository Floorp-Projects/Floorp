// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict";

// Checks that the security.OCSP.enabled pref correctly controls OCSP fetching
// behavior.

do_get_profile(); // Must be called before getting nsIX509CertDB
const gCertDB = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);

const SERVER_PORT = 8888;

function certFromFile(filename) {
  return constructCertFromFile(`test_ev_certs/${filename}.pem`);
}

function loadCert(certName, trustString) {
  addCertFromFile(gCertDB, `test_ev_certs/${certName}.pem`, trustString);
}

function getFailingOCSPResponder() {
  return getFailingHttpServer(SERVER_PORT, ["www.example.com"]);
}

function getOCSPResponder(expectedCertNames) {
  return startOCSPResponder(SERVER_PORT, "www.example.com", "test_ev_certs",
                            expectedCertNames, []);
}

// Tests that in ocspOff mode, OCSP fetches are never done.
function testOff() {
  add_test(() => {
    Services.prefs.setIntPref("security.OCSP.enabled", 0);
    do_print("Setting security.OCSP.enabled to 0");
    run_next_test();
  });

  // EV chains should verify successfully but never get EV status.
  add_test(() => {
    clearOCSPCache();
    let ocspResponder = getFailingOCSPResponder();
    checkEVStatus(gCertDB, certFromFile("test-oid-path-ee"), certificateUsageSSLServer,
                  false);
    ocspResponder.stop(run_next_test);
  });

  // A DV chain should verify successfully.
  add_test(() => {
    clearOCSPCache();
    let ocspResponder = getFailingOCSPResponder();
    checkCertErrorGeneric(gCertDB, certFromFile("non-ev-root-path-ee"),
                          PRErrorCodeSuccess, certificateUsageSSLServer);
    ocspResponder.stop(run_next_test);
  });
}

// Tests that in ocspOn mode, OCSP fetches are done for both EV and DV certs.
function testOn() {
  add_test(() => {
    Services.prefs.setIntPref("security.OCSP.enabled", 1);
    do_print("Setting security.OCSP.enabled to 1");
    run_next_test();
  });

  // If a successful OCSP response is fetched, then an EV chain should verify
  // successfully and get EV status as well.
  add_test(() => {
    clearOCSPCache();
    let ocspResponder =
      getOCSPResponder(gEVExpected ? ["test-oid-path-int", "test-oid-path-ee"]
                                   : ["test-oid-path-ee"]);
    checkEVStatus(gCertDB, certFromFile("test-oid-path-ee"), certificateUsageSSLServer,
                  gEVExpected);
    ocspResponder.stop(run_next_test);
  });

  // If a successful OCSP response is fetched, then a DV chain should verify
  // successfully.
  add_test(() => {
    clearOCSPCache();
    let ocspResponder = getOCSPResponder(["non-ev-root-path-ee"]);
    checkCertErrorGeneric(gCertDB, certFromFile("non-ev-root-path-ee"),
                          PRErrorCodeSuccess, certificateUsageSSLServer);
    ocspResponder.stop(run_next_test);
  });
}

// Tests that in ocspEVOnly mode, OCSP fetches are done for EV certs only.
function testEVOnly() {
  add_test(() => {
    Services.prefs.setIntPref("security.OCSP.enabled", 2);
    do_print("Setting security.OCSP.enabled to 2");
    run_next_test();
  });

  // If a successful OCSP response is fetched, then an EV chain should verify
  // successfully and get EV status as well.
  add_test(() => {
    clearOCSPCache();
    let ocspResponder = gEVExpected
                      ? getOCSPResponder(["test-oid-path-int", "test-oid-path-ee"])
                      : getFailingOCSPResponder();
    checkEVStatus(gCertDB, certFromFile("test-oid-path-ee"), certificateUsageSSLServer,
                  gEVExpected);
    ocspResponder.stop(run_next_test);
  });

  // A DV chain should verify successfully even without doing OCSP fetches.
  add_test(() => {
    clearOCSPCache();
    let ocspResponder = getFailingOCSPResponder();
    checkCertErrorGeneric(gCertDB, certFromFile("non-ev-root-path-ee"),
                          PRErrorCodeSuccess, certificateUsageSSLServer);
    ocspResponder.stop(run_next_test);
  });
}

function run_test() {
  do_register_cleanup(() => {
    Services.prefs.clearUserPref("network.dns.localDomains");
    Services.prefs.clearUserPref("security.OCSP.enabled");
    Services.prefs.clearUserPref("security.OCSP.require");
  });
  Services.prefs.setCharPref("network.dns.localDomains", "www.example.com");
  // Enable hard fail to ensure chains that should only succeed because they get
  // a good OCSP response do not succeed due to soft fail leniency.
  Services.prefs.setBoolPref("security.OCSP.require", true);

  loadCert("evroot", "CTu,,");
  loadCert("test-oid-path-int", ",,");
  loadCert("non-evroot-ca", "CTu,,");
  loadCert("non-ev-root-path-int", ",,");

  testOff();
  testOn();
  testEVOnly();

  run_next_test();
}
