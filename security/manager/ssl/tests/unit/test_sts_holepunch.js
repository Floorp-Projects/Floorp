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
  ok(!SSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS,
                            Services.io.newURI("https://chart.apis.google.com"),
                            0));
  ok(!SSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS,
                            Services.io.newURI("https://CHART.APIS.GOOGLE.COM"),
                            0));
  ok(!SSService.isSecureURI(
       Ci.nsISiteSecurityService.HEADER_HSTS,
       Services.io.newURI("https://sub.chart.apis.google.com"), 0));
  ok(!SSService.isSecureURI(
       Ci.nsISiteSecurityService.HEADER_HSTS,
       Services.io.newURI("https://SUB.CHART.APIS.GOOGLE.COM"), 0));
  ok(SSService.isSecureURI(
       Ci.nsISiteSecurityService.HEADER_HSTS,
       Services.io.newURI("https://example.apis.google.com"), 0));
  ok(SSService.isSecureURI(
       Ci.nsISiteSecurityService.HEADER_HSTS,
       Services.io.newURI("https://EXAMPLE.APIS.GOOGLE.COM"), 0));
  ok(SSService.isSecureURI(
       Ci.nsISiteSecurityService.HEADER_HSTS,
       Services.io.newURI("https://sub.example.apis.google.com"), 0));
  ok(SSService.isSecureURI(
       Ci.nsISiteSecurityService.HEADER_HSTS,
       Services.io.newURI("https://SUB.EXAMPLE.APIS.GOOGLE.COM"), 0));
}
