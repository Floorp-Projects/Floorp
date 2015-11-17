/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

// bug 961528: chart.apis.google.com doesn't handle https. Check that
// it isn't considered HSTS (other example.apis.google.com hosts should be
// HSTS as long as they're on the preload list, however).
function run_test() {
  let SSService = Cc["@mozilla.org/ssservice;1"]
                    .getService(Ci.nsISiteSecurityService);
  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "chart.apis.google.com", 0));
  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "CHART.APIS.GOOGLE.COM", 0));
  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "sub.chart.apis.google.com", 0));
  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "SUB.CHART.APIS.GOOGLE.COM", 0));
  ok(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                            "example.apis.google.com", 0));
  ok(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                            "EXAMPLE.APIS.GOOGLE.COM", 0));
  ok(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                            "sub.example.apis.google.com", 0));
  ok(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                            "SUB.EXAMPLE.APIS.GOOGLE.COM", 0));
  // also check isSecureURI
  let chartURI = Services.io.newURI("http://chart.apis.google.com", null, null);
  ok(!SSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS, chartURI, 0));
  let otherURI = Services.io.newURI("http://other.apis.google.com", null, null);
  ok(SSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS, otherURI, 0));
}
