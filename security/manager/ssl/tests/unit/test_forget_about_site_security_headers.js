/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Ensures that HSTS (HTTP Strict Transport Security) and HPKP (HTTP Public key
// pinning) are cleared when using "Forget About This Site".

var { ForgetAboutSite } = Cu.import("resource://gre/modules/ForgetAboutSite.jsm", {});

do_register_cleanup(() => {
  Services.prefs.clearUserPref("security.cert_pinning.enforcement_level");
  Services.prefs.clearUserPref(
    "security.cert_pinning.process_headers_from_non_builtin_roots");
});

const GOOD_MAX_AGE_SECONDS = 69403;
const NON_ISSUED_KEY_HASH = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
const PINNING_ROOT_KEY_HASH = "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8=";
const VALID_PIN = `pin-sha256="${PINNING_ROOT_KEY_HASH}";`;
const BACKUP_PIN = `pin-sha256="${NON_ISSUED_KEY_HASH}";`;
const GOOD_MAX_AGE = `max-age=${GOOD_MAX_AGE_SECONDS};`;

do_get_profile(); // must be done before instantiating nsIX509CertDB

Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 2);
Services.prefs.setBoolPref(
  "security.cert_pinning.process_headers_from_non_builtin_roots", true);

var certdb = Cc["@mozilla.org/security/x509certdb;1"]
               .getService(Ci.nsIX509CertDB);
addCertFromFile(certdb, "test_pinning_dynamic/pinningroot.pem", "CTu,CTu,CTu");

var sss = Cc["@mozilla.org/ssservice;1"]
            .getService(Ci.nsISiteSecurityService);
var uri = Services.io.newURI("https://a.pinning2.example.com");

// This test re-uses certificates from pinning tests because that's easier and
// simpler than recreating new certificates, hence the slightly longer than
// necessary domain name.
var sslStatus = new FakeSSLStatus(constructCertFromFile(
  "test_pinning_dynamic/a.pinning2.example.com-pinningroot.pem"));

// Test the normal case of processing HSTS and HPKP headers for
// a.pinning2.example.com, using "Forget About Site" on a.pinning2.example.com,
// and then checking that the platform doesn't consider a.pinning2.example.com
// to be HSTS or HPKP any longer.
add_task(function* () {
  sss.processHeader(Ci.nsISiteSecurityService.HEADER_HSTS, uri, GOOD_MAX_AGE,
                    sslStatus, 0);
  sss.processHeader(Ci.nsISiteSecurityService.HEADER_HPKP, uri,
                    GOOD_MAX_AGE + VALID_PIN + BACKUP_PIN, sslStatus, 0);

  Assert.ok(sss.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "a.pinning2.example.com", 0),
            "a.pinning2.example.com should be HSTS");
  Assert.ok(sss.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                             "a.pinning2.example.com", 0),
            "a.pinning2.example.com should be HPKP");

  yield ForgetAboutSite.removeDataFromDomain("a.pinning2.example.com");

  Assert.ok(!sss.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                              "a.pinning2.example.com", 0),
            "a.pinning2.example.com should not be HSTS now");
  Assert.ok(!sss.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                              "a.pinning2.example.com", 0),
            "a.pinning2.example.com should not be HPKP now");
});

// TODO (bug 1290529): the platform does not support this yet.
// Test the case of processing HSTS and HPKP headers for a.pinning2.example.com,
// using "Forget About Site" on example.com, and then checking that the platform
// doesn't consider the subdomain to be HSTS or HPKP any longer.
add_task(function* () {
  sss.processHeader(Ci.nsISiteSecurityService.HEADER_HSTS, uri, GOOD_MAX_AGE,
                    sslStatus, 0);
  sss.processHeader(Ci.nsISiteSecurityService.HEADER_HPKP, uri,
                    GOOD_MAX_AGE + VALID_PIN + BACKUP_PIN, sslStatus, 0);

  Assert.ok(sss.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "a.pinning2.example.com", 0),
            "a.pinning2.example.com should be HSTS (subdomain case)");
  Assert.ok(sss.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
                             "a.pinning2.example.com", 0),
            "a.pinning2.example.com should be HPKP (subdomain case)");

  yield ForgetAboutSite.removeDataFromDomain("example.com");

  // TODO (bug 1290529):
  // Assert.ok(!sss.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
  //                             "a.pinning2.example.com", 0),
  //           "a.pinning2.example.com should not be HSTS now (subdomain case)");
  // Assert.ok(!sss.isSecureHost(Ci.nsISiteSecurityService.HEADER_HPKP,
  //                             "a.pinning2.example.com", 0),
  //           "a.pinning2.example.com should not be HPKP now (subdomain case)");
});
