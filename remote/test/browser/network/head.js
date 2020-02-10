/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/remote/test/browser/head.js",
  this
);

function assertCookie(cookie, expected = {}) {
  const {
    name = "",
    value = "",
    domain = "example.org",
    path = "/",
    expires = -1,
    size = name.length + value.length,
    httpOnly = false,
    secure = false,
    session = true,
    sameSite,
  } = expected;

  const expectedCookie = {
    name,
    value,
    domain,
    path,
    expires,
    size,
    httpOnly,
    secure,
    session,
  };

  if (sameSite) {
    expectedCookie.sameSite = sameSite;
  }

  Assert.deepEqual(cookie, expectedCookie);
}

function getCookies() {
  return Services.cookies.cookies.map(cookie => {
    const data = {
      name: cookie.name,
      value: cookie.value,
      domain: cookie.host,
      path: cookie.path,
      expires: cookie.isSession ? -1 : cookie.expiry,
      // The size is the combined length of both the cookie name and value
      size: cookie.name.length + cookie.value.length,
      httpOnly: cookie.isHttpOnly,
      secure: cookie.isSecure,
      session: cookie.isSession,
    };

    if (cookie.sameSite) {
      const sameSiteMap = new Map([
        [Ci.nsICookie.SAMESITE_LAX, "Lax"],
        [Ci.nsICookie.SAMESITE_STRICT, "Strict"],
      ]);

      data.sameSite = sameSiteMap.get(cookie.sameSite);
    }

    return data;
  });
}
