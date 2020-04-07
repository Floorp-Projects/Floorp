"use strict";

function makeURI(str) {
  return Cc["@mozilla.org/network/io-service;1"]
    .getService(Ci.nsIIOService)
    .newURI(str);
}

function run_test() {
  // Allow all cookies.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  var serv = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);
  var uri = makeURI("http://example.com/");
  // Try an expiration time before the epoch
  serv.setCookieString(
    uri,
    "test=test; path=/; domain=example.com; expires=Sun, 31-Dec-1899 16:00:00 GMT;",
    null
  );
  Assert.equal(serv.getCookieString(uri, null), "");
  // Now sanity check
  serv.setCookieString(uri, "test2=test2; path=/; domain=example.com;", null);
  Assert.equal(serv.getCookieString(uri, null), "test2=test2");
}
