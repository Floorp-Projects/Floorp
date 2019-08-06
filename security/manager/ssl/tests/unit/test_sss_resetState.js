// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// Tests that resetting HSTS/HPKP state in the way the "forget about this site"
// functionality does works as expected for preloaded and non-preloaded sites.

do_get_profile();

var gSSService = Cc["@mozilla.org/ssservice;1"].getService(
  Ci.nsISiteSecurityService
);

function test_removeState(secInfo, type, flags) {
  info(`running test_removeState(type=${type}, flags=${flags})`);
  const NON_ISSUED_KEY_HASH = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
  const PINNING_ROOT_KEY_HASH = "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8=";
  const PINNING_HEADERS = `pin-sha256="${NON_ISSUED_KEY_HASH}"; pin-sha256="${PINNING_ROOT_KEY_HASH}"`;
  let headerAddendum =
    type == Ci.nsISiteSecurityService.HEADER_HPKP ? PINNING_HEADERS : "";
  // Simulate visiting a non-preloaded site by processing an HSTS or HPKP header
  // (depending on which type we were given), check that the HSTS/HPKP bit gets
  // set, simulate "forget about this site" (call removeState), and then check
  // that the HSTS/HPKP bit isn't set.
  let notPreloadedURI = Services.io.newURI("https://not-preloaded.example.com");
  ok(!gSSService.isSecureURI(type, notPreloadedURI, flags));
  gSSService.processHeader(
    type,
    notPreloadedURI,
    "max-age=1000;" + headerAddendum,
    secInfo,
    flags,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
  );
  ok(gSSService.isSecureURI(type, notPreloadedURI, flags));
  gSSService.resetState(type, notPreloadedURI, flags);
  ok(!gSSService.isSecureURI(type, notPreloadedURI, flags));

  // Simulate visiting a non-preloaded site that unsets HSTS/HPKP by processing
  // an HSTS/HPKP header with "max-age=0", check that the HSTS/HPKP bit isn't
  // set, simulate "forget about this site" (call removeState), and then check
  // that the HSTS/HPKP bit isn't set.
  gSSService.processHeader(
    type,
    notPreloadedURI,
    "max-age=0;" + headerAddendum,
    secInfo,
    flags,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
  );
  ok(!gSSService.isSecureURI(type, notPreloadedURI, flags));
  gSSService.resetState(type, notPreloadedURI, flags);
  ok(!gSSService.isSecureURI(type, notPreloadedURI, flags));

  // Simulate visiting a preloaded site by processing an HSTS/HPKP header, check
  // that the HSTS/HPKP bit is still set, simulate "forget about this site"
  // (call removeState), and then check that the HSTS/HPKP bit is still set.
  let preloadedHost =
    type == Ci.nsISiteSecurityService.HEADER_HPKP
      ? "include-subdomains.pinning.example.com"
      : "includesubdomains.preloaded.test";
  let preloadedURI = Services.io.newURI(`https://${preloadedHost}`);
  ok(gSSService.isSecureURI(type, preloadedURI, flags));
  gSSService.processHeader(
    type,
    preloadedURI,
    "max-age=1000;" + headerAddendum,
    secInfo,
    flags,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
  );
  ok(gSSService.isSecureURI(type, preloadedURI, flags));
  gSSService.resetState(type, preloadedURI, flags);
  ok(gSSService.isSecureURI(type, preloadedURI, flags));

  // Simulate visiting a preloaded site that unsets HSTS/HPKP by processing an
  // HSTS/HPKP header with "max-age=0", check that the HSTS/HPKP bit is what we
  // expect (see below), simulate "forget about this site" (call removeState),
  // and then check that the HSTS/HPKP bit is set.
  gSSService.processHeader(
    type,
    preloadedURI,
    "max-age=0;" + headerAddendum,
    secInfo,
    flags,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
  );
  // Due to architectural constraints, encountering a "max-age=0" header for a
  // preloaded HPKP site does not mark that site as not HPKP (whereas with HSTS,
  // it does).
  if (type == Ci.nsISiteSecurityService.HEADER_HPKP) {
    ok(gSSService.isSecureURI(type, preloadedURI, flags));
  } else {
    ok(!gSSService.isSecureURI(type, preloadedURI, flags));
  }
  gSSService.resetState(type, preloadedURI, flags);
  ok(gSSService.isSecureURI(type, preloadedURI, flags));
}

function add_tests() {
  let secInfo = null;
  add_connection_test(
    "not-preloaded.example.com",
    PRErrorCodeSuccess,
    undefined,
    aSecInfo => {
      secInfo = aSecInfo;
    }
  );

  add_task(() => {
    test_removeState(secInfo, Ci.nsISiteSecurityService.HEADER_HSTS, 0);
    test_removeState(
      secInfo,
      Ci.nsISiteSecurityService.HEADER_HSTS,
      Ci.nsISocketProvider.NO_PERMANENT_STORAGE
    );

    test_removeState(secInfo, Ci.nsISiteSecurityService.HEADER_HPKP, 0);
    test_removeState(
      secInfo,
      Ci.nsISiteSecurityService.HEADER_HPKP,
      Ci.nsISocketProvider.NO_PERMANENT_STORAGE
    );
  });
}

registerCleanupFunction(() => {
  Services.prefs.clearUserPref(
    "sercurity.cert_pinning.process_headers_from_non_builtin_roots"
  );
});

function run_test() {
  Services.prefs.setBoolPref(
    "security.cert_pinning.process_headers_from_non_builtin_roots",
    true
  );

  add_tls_server_setup("BadCertAndPinningServer", "bad_certs");

  add_tests();
  run_next_test();
}
