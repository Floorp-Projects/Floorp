// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
"use strict";

function check_ip(s, v, ip) {
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
  ok(!s.isSecureURI(uri));

  let parsedMaxAge = {};
  let parsedIncludeSubdomains = {};
  s.processHeader(
    uri,
    "max-age=1000;includeSubdomains",
    {},
    parsedMaxAge,
    parsedIncludeSubdomains
  );
  ok(
    !s.isSecureURI(uri),
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
