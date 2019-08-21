"use strict";

function check_ip(s, v, ip) {
  let secInfo = Cc[
    "@mozilla.org/security/transportsecurityinfo;1"
  ].createInstance(Ci.nsITransportSecurityInfo);

  let str = "https://";
  if (v == 6) {
    str += "[";
  }
  str += ip;
  if (v == 6) {
    str += "]";
  }
  str += "/";

  let uri = Services.io.newURI(str);
  ok(!s.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS, uri, 0));

  let parsedMaxAge = {};
  let parsedIncludeSubdomains = {};
  s.processHeader(
    Ci.nsISiteSecurityService.HEADER_HSTS,
    uri,
    "max-age=1000;includeSubdomains",
    secInfo,
    0,
    Ci.nsISiteSecurityService.SOURCE_ORGANIC_REQUEST,
    {},
    parsedMaxAge,
    parsedIncludeSubdomains
  );
  ok(
    !s.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS, uri, 0),
    "URI should not be secure if it contains an IP address"
  );

  /* Test that processHeader will ignore headers for an uri, if the uri
   * contains an IP address not a hostname.
   * If processHeader indeed ignore the header, then the output parameters will
   * remain empty, and we shouldn't see the values passed as the header.
   */
  notEqual(parsedMaxAge.value, 1000);
  notEqual(parsedIncludeSubdomains.value, true);
  notEqual(parsedMaxAge.value, undefined);
  notEqual(parsedIncludeSubdomains.value, undefined);
}

function run_test() {
  let SSService = Cc["@mozilla.org/ssservice;1"].getService(
    Ci.nsISiteSecurityService
  );

  check_ip(SSService, 4, "127.0.0.1");
  check_ip(SSService, 4, "10.0.0.1");
  check_ip(SSService, 6, "2001:db8::1");
  check_ip(SSService, 6, "1080::8:800:200C:417A");
}
