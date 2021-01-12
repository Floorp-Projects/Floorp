/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async () => {
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
    Ci.nsICookie.SAMESITE_NONE,
    Ci.nsICookie.SCHEME_HTTP
  );
  Assert.equal(cm.countCookiesFromHost("a"), 1);

  CookieXPCShellUtils.createServer({ hosts: ["a"] });
  const cookies = await CookieXPCShellUtils.getCookieStringFromDocument(
    "http://a/"
  );
  Assert.equal(cookies, "foo=bar");
});
