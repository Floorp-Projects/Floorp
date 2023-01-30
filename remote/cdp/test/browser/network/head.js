/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/remote/cdp/test/browser/head.js",
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

function assertEventOrder(first, second, options = {}) {
  const { ignoreTimestamps = false } = options;

  const firstDescription = getDescriptionForEvent(first);
  const secondDescription = getDescriptionForEvent(second);

  ok(
    first.index < second.index,
    `${firstDescription} received before ${secondDescription})`
  );

  if (!ignoreTimestamps) {
    ok(
      first.payload.timestamp <= second.payload.timestamp,
      `Timestamp of ${firstDescription}) is earlier than ${secondDescription})`
    );
  }
}

function filterEventsByType(history, type) {
  return history.filter(event => event.payload.type == type);
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

function getDescriptionForEvent(event) {
  const { eventName, payload } = event;

  return `${eventName}(${payload.type || payload.name || payload.frameId})`;
}
