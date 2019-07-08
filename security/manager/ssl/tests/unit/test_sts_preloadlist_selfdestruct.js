"use strict";

function run_test() {
  let SSService = Cc["@mozilla.org/ssservice;1"].getService(
    Ci.nsISiteSecurityService
  );
  let uri = Services.io.newURI("https://includesubdomains.preloaded.test");

  // check that a host on the preload list is identified as an sts host
  ok(SSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS, uri, 0));

  // now simulate that it's 19 weeks later than it actually is
  let offsetSeconds = 19 * 7 * 24 * 60 * 60;
  Services.prefs.setIntPref("test.currentTimeOffsetSeconds", offsetSeconds);

  // check that the preloaded host is no longer considered sts
  ok(!SSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS, uri, 0));

  // just make sure we can get everything back to normal
  Services.prefs.clearUserPref("test.currentTimeOffsetSeconds");
  ok(SSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS, uri, 0));
}
