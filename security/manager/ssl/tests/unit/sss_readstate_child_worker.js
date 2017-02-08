/* import-globals-from head_psm.js */
"use strict";

function run_test() {
  let SSService = Cc["@mozilla.org/ssservice;1"]
                    .getService(Ci.nsISiteSecurityService);

  ok(!SSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS,
                            Services.io.newURI("https://expired.example.com"),
                            0));
  ok(SSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS,
                           Services.io.newURI("https://notexpired.example.com"),
                           0));
  ok(SSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS,
                           Services.io.newURI("https://bugzilla.mozilla.org"),
                           0));
  ok(!SSService.isSecureURI(
       Ci.nsISiteSecurityService.HEADER_HSTS,
       Services.io.newURI("https://sub.bugzilla.mozilla.org"), 0));
  ok(SSService.isSecureURI(
       Ci.nsISiteSecurityService.HEADER_HSTS,
       Services.io.newURI("https://incsubdomain.example.com"), 0));
  ok(SSService.isSecureURI(
       Ci.nsISiteSecurityService.HEADER_HSTS,
       Services.io.newURI("https://sub.incsubdomain.example.com"), 0));
  ok(!SSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS,
                            Services.io.newURI("https://login.persona.org"),
                            0));
  ok(!SSService.isSecureURI(Ci.nsISiteSecurityService.HEADER_HSTS,
                            Services.io.newURI("https://sub.login.persona.org"),
                            0));
  do_test_finished();
}
