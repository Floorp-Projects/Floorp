/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async () => {
  Services.prefs.setBoolPref("network.cookie.sameSite.schemeful", false);

  var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);
  var expiry = (Date.now() + 1000) * 1000;

  // Test our handling of host names with a single character at the beginning
  // followed by a dot.
  cm.add(
    "e.com",
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
  Assert.equal(cm.countCookiesFromHost("e.com"), 1);

  CookieXPCShellUtils.createServer({ hosts: ["e.com"] });
  const cookies = await CookieXPCShellUtils.getCookieStringFromDocument(
    "http://e.com/"
  );
  Assert.equal(cookies, "foo=bar");
});
