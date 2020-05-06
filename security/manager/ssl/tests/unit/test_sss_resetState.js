// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

// Tests that resetting HSTS state in the way the "forget about this site"
// functionality does works as expected for preloaded and non-preloaded sites.

do_get_profile();

var gSSService = Cc["@mozilla.org/ssservice;1"].getService(
  Ci.nsISiteSecurityService
);

function test_removeState(secInfo, type, flags) {
  info(`running test_removeState(type=${type}, flags=${flags})`);
  // Simulate visiting a non-preloaded site by processing an HSTS header check
  // that the HSTS bit gets set, simulate "forget about this site" (call
  // removeState), and then check that the HSTS bit isn't set.
  let notPreloadedURI = Services.io.newURI("https://not-preloaded.example.com");
  ok(!gSSService.isSecureURI(type, notPreloadedURI, flags));
  gSSService.processHeader(
    type,
    notPreloadedURI,
    "max-age=1000;",
    secInfo,
    flags,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
  );
  ok(gSSService.isSecureURI(type, notPreloadedURI, flags));
  gSSService.resetState(type, notPreloadedURI, flags);
  ok(!gSSService.isSecureURI(type, notPreloadedURI, flags));

  // Simulate visiting a non-preloaded site that unsets HSTS by processing
  // an HSTS header with "max-age=0", check that the HSTS bit isn't
  // set, simulate "forget about this site" (call removeState), and then check
  // that the HSTS bit isn't set.
  gSSService.processHeader(
    type,
    notPreloadedURI,
    "max-age=0;",
    secInfo,
    flags,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
  );
  ok(!gSSService.isSecureURI(type, notPreloadedURI, flags));
  gSSService.resetState(type, notPreloadedURI, flags);
  ok(!gSSService.isSecureURI(type, notPreloadedURI, flags));

  // Simulate visiting a preloaded site by processing an HSTS header, check
  // that the HSTS bit is still set, simulate "forget about this site"
  // (call removeState), and then check that the HSTS bit is still set.
  let preloadedHost = "includesubdomains.preloaded.test";
  let preloadedURI = Services.io.newURI(`https://${preloadedHost}`);
  ok(gSSService.isSecureURI(type, preloadedURI, flags));
  gSSService.processHeader(
    type,
    preloadedURI,
    "max-age=1000;",
    secInfo,
    flags,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
  );
  ok(gSSService.isSecureURI(type, preloadedURI, flags));
  gSSService.resetState(type, preloadedURI, flags);
  ok(gSSService.isSecureURI(type, preloadedURI, flags));

  // Simulate visiting a preloaded site that unsets HSTS by processing an
  // HSTS header with "max-age=0", check that the HSTS bit is what we
  // expect (see below), simulate "forget about this site" (call removeState),
  // and then check that the HSTS bit is set.
  gSSService.processHeader(
    type,
    preloadedURI,
    "max-age=0;",
    secInfo,
    flags,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST
  );
  ok(!gSSService.isSecureURI(type, preloadedURI, flags));
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
  });
}

function run_test() {
  add_tls_server_setup("BadCertAndPinningServer", "bad_certs");

  add_tests();
  run_next_test();
}
