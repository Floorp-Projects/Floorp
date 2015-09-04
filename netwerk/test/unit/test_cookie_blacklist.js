const GOOD_COOKIE = "GoodCookie=OMNOMNOM";

function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var cookieURI = ios.newURI("http://mozilla.org/test_cookie_blacklist.js",
                             null, null);

  var cookieService = Cc["@mozilla.org/cookieService;1"]
                        .getService(Ci.nsICookieService);
  cookieService.setCookieString(cookieURI, null, "BadCookie1=\x01", null);
  cookieService.setCookieString(cookieURI, null, "BadCookie2=\v", null);
  cookieService.setCookieString(cookieURI, null, GOOD_COOKIE, null);

  var storedCookie = cookieService.getCookieString(cookieURI, null);
  do_check_eq(storedCookie, GOOD_COOKIE);
}
