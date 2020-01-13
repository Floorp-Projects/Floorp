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
    // If expires is set, convert from milliseconds to seconds
    expires: expires > 0 ? Math.floor(expires.getTime() / 1000) : -1,
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
