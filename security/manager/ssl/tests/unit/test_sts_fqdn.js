/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

function run_test() {
  let SSService = Cc["@mozilla.org/ssservice;1"]
                    .getService(Ci.nsISiteSecurityService);
  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "example.com", 0));
  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "example.com.", 0));
  // These cases are only relevant as long as bug 1118522 hasn't been fixed.
  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "example.com..", 0));

  let uri = Services.io.newURI("https://example.com");
  let sslStatus = new FakeSSLStatus();
  SSService.processHeader(Ci.nsISiteSecurityService.HEADER_HSTS, uri,
                          "max-age=1000;includeSubdomains", sslStatus, 0);
  ok(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                            "example.com", 0));
  ok(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                            "example.com.", 0));
  ok(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                            "example.com..", 0));

  ok(SSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS, uri, 0));
  uri = Services.io.newURI("https://example.com.");
  ok(SSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS, uri, 0));
  uri = Services.io.newURI("https://example.com..");
  ok(SSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS, uri, 0));

  SSService.removeState(Ci.nsISiteSecurityService.HEADER_HSTS, uri, 0);
  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "example.com", 0));
  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "example.com.", 0));
  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "example.com..", 0));

  // Somehow creating this malformed URI succeeds - we need to handle it
  // gracefully.
  uri = Services.io.newURI("https://../foo");
  equal(uri.host, "..");
  throws(() => {
    SSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS, uri, 0);
  }, /NS_ERROR_UNEXPECTED/, "Malformed URI should be rejected");
}
