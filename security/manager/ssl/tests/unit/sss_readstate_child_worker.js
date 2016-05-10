/* import-globals-from head_psm.js */
"use strict";

function run_test() {
  let SSService = Cc["@mozilla.org/ssservice;1"]
                    .getService(Ci.nsISiteSecurityService);

  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "expired.example.com", 0));
  ok(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                            "notexpired.example.com", 0));
  ok(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                            "bugzilla.mozilla.org", 0));
  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "sub.bugzilla.mozilla.org", 0));
  ok(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                            "incsubdomain.example.com", 0));
  ok(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                            "sub.incsubdomain.example.com", 0));
  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "login.persona.org", 0));
  ok(!SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                             "sub.login.persona.org", 0));
  do_test_finished();
}
