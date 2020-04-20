/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function run_test() {
  var cs = Cc["@mozilla.org/cookieService;1"].getService(Ci.nsICookieService);
  var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);
  var expiry = (Date.now() + 1000) * 1000;

  // Test our handling of host names with a single character consisting only
  // of a single character
  cm.add(
    "a",
    "/",
    "foo",
    "bar",
    false,
    false,
    true,
    expiry,
    {},
    Ci.nsICookie.SAMESITE_NONE
  );
  Assert.equal(cm.countCookiesFromHost("a"), 1);

  const uri = NetUtil.newURI("http://a");
  const principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );

  Assert.equal(cs.getCookieStringForPrincipal(principal), "foo=bar");
}
