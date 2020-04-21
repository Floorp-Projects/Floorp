"use strict";

const GOOD_COOKIE = "GoodCookie=OMNOMNOM";
const SPACEY_COOKIE = "Spacey Cookie=Major Tom";

function run_test() {
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var cookieURI = ios.newURI("http://mozilla.org/test_cookie_blacklist.js");
  const channel = NetUtil.newChannel({
    uri: cookieURI,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  });

  var cookieService = Cc["@mozilla.org/cookieService;1"].getService(
    Ci.nsICookieService
  );
  cookieService.setCookieStringFromHttp(cookieURI, "BadCookie1=\x01", channel);
  cookieService.setCookieStringFromHttp(cookieURI, "BadCookie2=\v", channel);
  cookieService.setCookieStringFromHttp(
    cookieURI,
    "Bad\x07Name=illegal",
    channel
  );
  cookieService.setCookieStringFromHttp(cookieURI, GOOD_COOKIE, channel);
  cookieService.setCookieStringFromHttp(cookieURI, SPACEY_COOKIE, channel);

  const principal = Services.scriptSecurityManager.createContentPrincipal(
    cookieURI,
    {}
  );
  var storedCookie = cookieService.getCookieStringForPrincipal(principal);

  Assert.equal(storedCookie, GOOD_COOKIE + "; " + SPACEY_COOKIE);
}
