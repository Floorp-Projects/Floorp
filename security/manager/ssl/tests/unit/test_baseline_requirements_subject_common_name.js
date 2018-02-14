// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The preference security.pki.name_matching_mode controls whether or not
// mozilla::pkix will fall back to using a certificate's subject common name
// during name matching. If the Baseline Requirements are followed, fallback
// should not be necessary (because any name information in the subject common
// name should be present in the subject alternative name extension). Due to
// compatibility concerns, the platform can be configured to fall back for
// certificates that are valid before 23 August 2016. Note that for certificates
// issued by an imported root, the platform will fall back if necessary,
// regardless of the value of the preference.

"use strict";

do_get_profile(); // must be called before getting nsIX509CertDB
const gCertDB = Cc["@mozilla.org/security/x509certdb;1"]
                  .getService(Ci.nsIX509CertDB);

function certFromFile(certName) {
  return constructCertFromFile(`test_baseline_requirements/${certName}.pem`);
}

function loadCertWithTrust(certName, trustString) {
  addCertFromFile(gCertDB, `test_baseline_requirements/${certName}.pem`,
                  trustString);
}

function checkCertOn25August2016(cert, expectedResult) {
  // (new Date("2016-08-25T00:00:00Z")).getTime() / 1000
  const VALIDATION_TIME = 1472083200;
  checkCertErrorGenericAtTime(gCertDB, cert, expectedResult,
                              certificateUsageSSLServer, VALIDATION_TIME, {},
                              "example.com");
}

function run_test() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("security.pki.name_matching_mode");
    Services.prefs.clearUserPref("security.test.built_in_root_hash");
  });

  loadCertWithTrust("ca", "CTu,,");

  // When verifying a certificate, if the trust anchor is not a built-in root,
  // name matching will fall back to using the subject common name if necessary
  // (i.e. if there is no subject alternative name extension or it does not
  // contain any dNSName or iPAddress entries). Thus, since imported roots are
  // not in general treated as built-ins, these should all successfully verify
  // regardless of the value of the pref.
  Services.prefs.setIntPref("security.pki.name_matching_mode", 0);
  info("current mode: always fall back, root not built-in");
  checkCertOn25August2016(certFromFile("no-san-recent"),
                          PRErrorCodeSuccess);
  checkCertOn25August2016(certFromFile("no-san-old"),
                          PRErrorCodeSuccess);
  checkCertOn25August2016(certFromFile("no-san-older"),
                          PRErrorCodeSuccess);
  checkCertOn25August2016(certFromFile("san-contains-no-hostnames-recent"),
                          PRErrorCodeSuccess);
  checkCertOn25August2016(certFromFile("san-contains-no-hostnames-old"),
                          PRErrorCodeSuccess);
  checkCertOn25August2016(certFromFile("san-contains-no-hostnames-older"),
                          PRErrorCodeSuccess);

  Services.prefs.setIntPref("security.pki.name_matching_mode", 1);
  info("current mode: fall back for notBefore < August 23, 2016, root " +
       "not built-in");
  checkCertOn25August2016(certFromFile("no-san-recent"),
                          PRErrorCodeSuccess);
  checkCertOn25August2016(certFromFile("no-san-old"),
                          PRErrorCodeSuccess);
  checkCertOn25August2016(certFromFile("no-san-older"),
                          PRErrorCodeSuccess);
  checkCertOn25August2016(certFromFile("san-contains-no-hostnames-recent"),
                          PRErrorCodeSuccess);
  checkCertOn25August2016(certFromFile("san-contains-no-hostnames-old"),
                          PRErrorCodeSuccess);
  checkCertOn25August2016(certFromFile("san-contains-no-hostnames-older"),
                          PRErrorCodeSuccess);

  Services.prefs.setIntPref("security.pki.name_matching_mode", 2);
  info("current mode: fall back for notBefore < August 23, 2015, root " +
       "not built-in");
  checkCertOn25August2016(certFromFile("no-san-recent"),
                          PRErrorCodeSuccess);
  checkCertOn25August2016(certFromFile("no-san-old"),
                          PRErrorCodeSuccess);
  checkCertOn25August2016(certFromFile("no-san-older"),
                          PRErrorCodeSuccess);
  checkCertOn25August2016(certFromFile("san-contains-no-hostnames-recent"),
                          PRErrorCodeSuccess);
  checkCertOn25August2016(certFromFile("san-contains-no-hostnames-old"),
                          PRErrorCodeSuccess);
  checkCertOn25August2016(certFromFile("san-contains-no-hostnames-older"),
                          PRErrorCodeSuccess);

  Services.prefs.setIntPref("security.pki.name_matching_mode", 3);
  info("current mode: never fall back, root not built-in");
  checkCertOn25August2016(certFromFile("no-san-recent"),
                          PRErrorCodeSuccess);
  checkCertOn25August2016(certFromFile("no-san-old"),
                          PRErrorCodeSuccess);
  checkCertOn25August2016(certFromFile("no-san-older"),
                          PRErrorCodeSuccess);
  checkCertOn25August2016(certFromFile("san-contains-no-hostnames-recent"),
                          PRErrorCodeSuccess);
  checkCertOn25August2016(certFromFile("san-contains-no-hostnames-old"),
                          PRErrorCodeSuccess);
  checkCertOn25August2016(certFromFile("san-contains-no-hostnames-older"),
                          PRErrorCodeSuccess);

  // In debug builds, we can treat an imported root as a built-in, and thus we
  // can actually test the different values of the pref.
  if (isDebugBuild) {
    let root = certFromFile("ca");
    Services.prefs.setCharPref("security.test.built_in_root_hash",
                               root.sha256Fingerprint);

    // Always fall back if necessary.
    Services.prefs.setIntPref("security.pki.name_matching_mode", 0);
    info("current mode: always fall back, root built-in");
    checkCertOn25August2016(certFromFile("no-san-recent"),
                            PRErrorCodeSuccess);
    checkCertOn25August2016(certFromFile("no-san-old"),
                            PRErrorCodeSuccess);
    checkCertOn25August2016(certFromFile("no-san-older"),
                            PRErrorCodeSuccess);
    checkCertOn25August2016(certFromFile("san-contains-no-hostnames-recent"),
                            PRErrorCodeSuccess);
    checkCertOn25August2016(certFromFile("san-contains-no-hostnames-old"),
                            PRErrorCodeSuccess);
    checkCertOn25August2016(certFromFile("san-contains-no-hostnames-older"),
                            PRErrorCodeSuccess);

    // Only fall back if notBefore < 23 August 2016
    Services.prefs.setIntPref("security.pki.name_matching_mode", 1);
    info("current mode: fall back for notBefore < August 23, 2016, root " +
         "built-in");
    checkCertOn25August2016(certFromFile("no-san-recent"),
                            SSL_ERROR_BAD_CERT_DOMAIN);
    checkCertOn25August2016(certFromFile("no-san-old"),
                            PRErrorCodeSuccess);
    checkCertOn25August2016(certFromFile("no-san-older"),
                            PRErrorCodeSuccess);
    checkCertOn25August2016(certFromFile("san-contains-no-hostnames-recent"),
                            SSL_ERROR_BAD_CERT_DOMAIN);
    checkCertOn25August2016(certFromFile("san-contains-no-hostnames-old"),
                            PRErrorCodeSuccess);
    checkCertOn25August2016(certFromFile("san-contains-no-hostnames-older"),
                            PRErrorCodeSuccess);

    // Only fall back if notBefore < 23 August 2015
    Services.prefs.setIntPref("security.pki.name_matching_mode", 2);
    info("current mode: fall back for notBefore < August 23, 2015, root " +
         "built-in");
    checkCertOn25August2016(certFromFile("no-san-recent"),
                            SSL_ERROR_BAD_CERT_DOMAIN);
    checkCertOn25August2016(certFromFile("no-san-old"),
                            SSL_ERROR_BAD_CERT_DOMAIN);
    checkCertOn25August2016(certFromFile("no-san-older"),
                            PRErrorCodeSuccess);
    checkCertOn25August2016(certFromFile("san-contains-no-hostnames-recent"),
                            SSL_ERROR_BAD_CERT_DOMAIN);
    checkCertOn25August2016(certFromFile("san-contains-no-hostnames-old"),
                            SSL_ERROR_BAD_CERT_DOMAIN);
    checkCertOn25August2016(certFromFile("san-contains-no-hostnames-older"),
                            PRErrorCodeSuccess);

    // Never fall back.
    Services.prefs.setIntPref("security.pki.name_matching_mode", 3);
    info("current mode: never fall back, root built-in");
    checkCertOn25August2016(certFromFile("no-san-recent"),
                            SSL_ERROR_BAD_CERT_DOMAIN);
    checkCertOn25August2016(certFromFile("no-san-old"),
                            SSL_ERROR_BAD_CERT_DOMAIN);
    checkCertOn25August2016(certFromFile("no-san-older"),
                            SSL_ERROR_BAD_CERT_DOMAIN);
    checkCertOn25August2016(certFromFile("san-contains-no-hostnames-recent"),
                            SSL_ERROR_BAD_CERT_DOMAIN);
    checkCertOn25August2016(certFromFile("san-contains-no-hostnames-old"),
                            SSL_ERROR_BAD_CERT_DOMAIN);
    checkCertOn25August2016(certFromFile("san-contains-no-hostnames-older"),
                            SSL_ERROR_BAD_CERT_DOMAIN);
  }
}
