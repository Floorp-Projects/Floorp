const GOOD_COOKIE = "GoodCookie=OMNOMNOM";
const SPACEY_COOKIE = "Spacey Cookie=Major Tom";

function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var cookieURI = ios.newURI("http://mozilla.org/test_cookie_blacklist.js");

  var cookieService = Cc["@mozilla.org/cookieService;1"]
                        .getService(Ci.nsICookieService);
  cookieService.setCookieStringFromHttp(cookieURI, cookieURI, null, "BadCookie1=\x01", null, null);
  cookieService.setCookieStringFromHttp(cookieURI, cookieURI, null, "BadCookie2=\v", null, null);
  cookieService.setCookieStringFromHttp(cookieURI, cookieURI, null, "Bad\x07Name=illegal", null, null);
  cookieService.setCookieStringFromHttp(cookieURI, cookieURI, null, GOOD_COOKIE, null, null);
  cookieService.setCookieStringFromHttp(cookieURI, cookieURI, null, SPACEY_COOKIE, null, null);

  var storedCookie = cookieService.getCookieString(cookieURI, null);
  do_check_eq(storedCookie, GOOD_COOKIE + "; " + SPACEY_COOKIE);
}
