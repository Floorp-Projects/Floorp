/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async () => {
  Services.prefs.setBoolPref("dom.security.https_first", false);
  var expiry = (Date.now() + 1000) * 1000;

  // Test our handling of host names with a single character consisting only
  // of a single character
  Services.cookies.add(
    "a",
    "/",
    "foo",
    "bar",
    false,
    false,
    true,
    expiry,
    {},
    Ci.nsICookie.SAMESITE_NONE,
    Ci.nsICookie.SCHEME_HTTP
  );
  Assert.equal(Services.cookies.countCookiesFromHost("a"), 1);

  CookieXPCShellUtils.createServer({ hosts: ["a"] });
  const cookies = await CookieXPCShellUtils.getCookieStringFromDocument(
    "http://a/"
  );
  Assert.equal(cookies, "foo=bar");
  Services.prefs.clearUserPref("dom.security.https_first");
});
