"use strict";

const GOOD_COOKIE = "GoodCookie=OMNOMNOM";
const SPACEY_COOKIE = "Spacey Cookie=Major Tom";

function run_test() {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var cookieURI = ios.newURI("http://mozilla.org/test_cookie_blacklist.js");

  var cookieService = Cc["@mozilla.org/cookieService;1"].getService(
    Ci.nsICookieService
  );
  cookieService.setCookieStringFromHttp(
    cookieURI,
    cookieURI,
    "BadCookie1=\x01",
    null,
    null
  );
  cookieService.setCookieStringFromHttp(
    cookieURI,
    cookieURI,
    "BadCookie2=\v",
    null,
    null
  );
  cookieService.setCookieStringFromHttp(
    cookieURI,
    cookieURI,
    "Bad\x07Name=illegal",
    null,
    null
  );
  cookieService.setCookieStringFromHttp(
    cookieURI,
    cookieURI,
    GOOD_COOKIE,
    null,
    null
  );
  cookieService.setCookieStringFromHttp(
    cookieURI,
    cookieURI,
    SPACEY_COOKIE,
    null,
    null
  );

  const principal = Services.scriptSecurityManager.createContentPrincipal(
    cookieURI,
    {}
  );
  var storedCookie = cookieService.getCookieStringForPrincipal(principal);

  Assert.equal(storedCookie, GOOD_COOKIE + "; " + SPACEY_COOKIE);
}
