function run_test() {
  var SSService = Cc["@mozilla.org/ssservice;1"]
                    .getService(Ci.nsISiteSecurityService);

  do_check_false(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                                        "expired.example.com", 0));
  do_check_true(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                                       "notexpired.example.com", 0));
  do_check_true(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                                       "bugzilla.mozilla.org", 0));
  do_check_false(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                                        "sub.bugzilla.mozilla.org", 0));
  do_check_true(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                                       "incsubdomain.example.com", 0));
  do_check_true(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                                       "sub.incsubdomain.example.com", 0));
  do_check_false(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                                        "login.persona.org", 0));
  do_check_false(SSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS,
                                        "sub.login.persona.org", 0));
  do_test_finished();
}
