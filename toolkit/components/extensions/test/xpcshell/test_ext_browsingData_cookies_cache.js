/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

const { SiteDataTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SiteDataTestUtils.sys.mjs"
);

const COOKIE = {
  host: "example.com",
  name: "test_cookie",
  path: "/",
};
const COOKIE_NET = {
  host: "example.net",
  name: "test_cookie",
  path: "/",
};
const COOKIE_ORG = {
  host: "example.org",
  name: "test_cookie",
  path: "/",
};
let since, oldCookie;

function addCookie(cookie) {
  Services.cookies.add(
    cookie.host,
    cookie.path,
    cookie.name,
    "test",
    false,
    false,
    false,
    Date.now() / 1000 + 10000,
    {},
    Ci.nsICookie.SAMESITE_NONE,
    Ci.nsICookie.SCHEME_HTTPS
  );
  ok(
    Services.cookies.cookieExists(cookie.host, cookie.path, cookie.name, {}),
    `Cookie ${cookie.name} was created.`
  );
}

async function setUpCookies() {
  Services.cookies.removeAll();

  // Add a cookie which will end up with an older creationTime.
  oldCookie = Object.assign({}, COOKIE, { name: Date.now() });
  addCookie(oldCookie);
  await new Promise(resolve => setTimeout(resolve, 10));
  since = Date.now();
  await new Promise(resolve => setTimeout(resolve, 10));

  // Add a cookie which will end up with a more recent creationTime.
  addCookie(COOKIE);

  // Add cookies for different domains.
  addCookie(COOKIE_NET);
  addCookie(COOKIE_ORG);
}

async function setUpCache() {
  Services.cache2.clear();

  // Add cache entries for different domains.
  for (const domain of ["example.net", "example.org", "example.com"]) {
    await SiteDataTestUtils.addCacheEntry(`http://${domain}/`, "disk");
    await SiteDataTestUtils.addCacheEntry(`http://${domain}/`, "memory");
  }
}

function hasCacheEntry(domain) {
  const disk = SiteDataTestUtils.hasCacheEntry(`http://${domain}/`, "disk");
  const memory = SiteDataTestUtils.hasCacheEntry(`http://${domain}/`, "memory");

  equal(
    disk,
    memory,
    `For ${domain} either either both or neither caches need to exists.`
  );
  return disk;
}

add_task(async function testCache() {
  function background() {
    browser.test.onMessage.addListener(async msg => {
      if (msg == "removeCache") {
        await browser.browsingData.removeCache({});
      } else {
        await browser.browsingData.remove({}, { cache: true });
      }
      browser.test.sendMessage("cacheRemoved");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["browsingData"],
    },
  });

  async function testRemovalMethod(method) {
    await setUpCache();

    extension.sendMessage(method);
    await extension.awaitMessage("cacheRemoved");

    ok(!hasCacheEntry("example.net"), "example.net cache was removed");
    ok(!hasCacheEntry("example.org"), "example.org cache was removed");
    ok(!hasCacheEntry("example.com"), "example.com cache was removed");
  }

  await extension.startup();

  await testRemovalMethod("removeCache");
  await testRemovalMethod("remove");

  await extension.unload();
});

add_task(async function testCookies() {
  // Above in setUpCookies we create an 'old' cookies, wait 10ms, then log a timestamp.
  // Here we ask the browser to delete all cookies after the timestamp, with the intention
  // that the 'old' cookie is not removed. The issue arises when the timer precision is
  // low enough such that the timestamp that gets logged is the same as the 'old' cookie.
  // We hardcode a precision value to ensure that there is time between the 'old' cookie
  // and the timestamp generation.
  Services.prefs.setBoolPref("privacy.reduceTimerPrecision", true);
  Services.prefs.setIntPref(
    "privacy.resistFingerprinting.reduceTimerPrecision.microseconds",
    2000
  );

  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("privacy.reduceTimerPrecision");
    Services.prefs.clearUserPref(
      "privacy.resistFingerprinting.reduceTimerPrecision.microseconds"
    );
  });

  function background() {
    browser.test.onMessage.addListener(async (msg, options) => {
      if (msg == "removeCookies") {
        await browser.browsingData.removeCookies(options);
      } else {
        await browser.browsingData.remove(options, { cookies: true });
      }
      browser.test.sendMessage("cookiesRemoved");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["browsingData"],
    },
  });

  async function testRemovalMethod(method) {
    // Clear cookies with a recent since value.
    await setUpCookies();
    extension.sendMessage(method, { since });
    await extension.awaitMessage("cookiesRemoved");

    ok(
      Services.cookies.cookieExists(
        oldCookie.host,
        oldCookie.path,
        oldCookie.name,
        {}
      ),
      "Old cookie was not removed."
    );
    ok(
      !Services.cookies.cookieExists(COOKIE.host, COOKIE.path, COOKIE.name, {}),
      "Recent cookie was removed."
    );

    // Clear cookies with an old since value.
    await setUpCookies();
    addCookie(COOKIE);
    extension.sendMessage(method, { since: since - 100000 });
    await extension.awaitMessage("cookiesRemoved");

    ok(
      !Services.cookies.cookieExists(
        oldCookie.host,
        oldCookie.path,
        oldCookie.name,
        {}
      ),
      "Old cookie was removed."
    );
    ok(
      !Services.cookies.cookieExists(COOKIE.host, COOKIE.path, COOKIE.name, {}),
      "Recent cookie was removed."
    );

    // Clear cookies with no since value and valid originTypes.
    await setUpCookies();
    extension.sendMessage(method, {
      originTypes: { unprotectedWeb: true, protectedWeb: false },
    });
    await extension.awaitMessage("cookiesRemoved");

    ok(
      !Services.cookies.cookieExists(COOKIE.host, COOKIE.path, COOKIE.name, {}),
      `Cookie ${COOKIE.name}  was removed.`
    );
    ok(
      !Services.cookies.cookieExists(
        oldCookie.host,
        oldCookie.path,
        oldCookie.name,
        {}
      ),
      `Cookie ${oldCookie.name}  was removed.`
    );
  }

  await extension.startup();

  await testRemovalMethod("removeCookies");
  await testRemovalMethod("remove");

  await extension.unload();
});

add_task(async function testCacheAndCookies() {
  function background() {
    browser.test.onMessage.addListener(async options => {
      await browser.browsingData.remove(options, {
        cache: true,
        cookies: true,
      });
      browser.test.sendMessage("cacheAndCookiesRemoved");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["browsingData"],
    },
  });

  await extension.startup();

  // Clear cache and cookies with a recent since value.
  await setUpCookies();
  await setUpCache();
  extension.sendMessage({ since });
  await extension.awaitMessage("cacheAndCookiesRemoved");

  ok(
    Services.cookies.cookieExists(
      oldCookie.host,
      oldCookie.path,
      oldCookie.name,
      {}
    ),
    "Old cookie was not removed."
  );
  ok(
    !Services.cookies.cookieExists(COOKIE.host, COOKIE.path, COOKIE.name, {}),
    "Recent cookie was removed."
  );

  // Cache does not support |since| and deletes everything!
  ok(!hasCacheEntry("example.net"), "example.net cache was removed");
  ok(!hasCacheEntry("example.org"), "example.org cache was removed");
  ok(!hasCacheEntry("example.com"), "example.com cache was removed");

  // Clear cache and cookies with an old since value.
  await setUpCookies();
  await setUpCache();
  extension.sendMessage({ since: since - 100000 });
  await extension.awaitMessage("cacheAndCookiesRemoved");

  // Cache does not support |since| and deletes everything!
  ok(!hasCacheEntry("example.net"), "example.net cache was removed");
  ok(!hasCacheEntry("example.org"), "example.org cache was removed");
  ok(!hasCacheEntry("example.com"), "example.com cache was removed");

  ok(
    !Services.cookies.cookieExists(
      oldCookie.host,
      oldCookie.path,
      oldCookie.name,
      {}
    ),
    "Old cookie was removed."
  );
  ok(
    !Services.cookies.cookieExists(COOKIE.host, COOKIE.path, COOKIE.name, {}),
    "Recent cookie was removed."
  );

  // Clear cache and cookies with hostnames value.
  await setUpCookies();
  await setUpCache();
  extension.sendMessage({
    hostnames: ["example.net", "example.org", "unknown.com"],
  });
  await extension.awaitMessage("cacheAndCookiesRemoved");

  ok(
    Services.cookies.cookieExists(COOKIE.host, COOKIE.path, COOKIE.name, {}),
    `Cookie ${COOKIE.name}  was not removed.`
  );
  ok(
    !Services.cookies.cookieExists(
      COOKIE_NET.host,
      COOKIE_NET.path,
      COOKIE_NET.name,
      {}
    ),
    `Cookie ${COOKIE_NET.name}  was removed.`
  );
  ok(
    !Services.cookies.cookieExists(
      COOKIE_ORG.host,
      COOKIE_ORG.path,
      COOKIE_ORG.name,
      {}
    ),
    `Cookie ${COOKIE_ORG.name}  was removed.`
  );

  ok(!hasCacheEntry("example.net"), "example.net cache was removed");
  ok(!hasCacheEntry("example.org"), "example.org cache was removed");
  ok(hasCacheEntry("example.com"), "example.com cache was not removed");

  // Clear cache and cookies with (empty) hostnames value.
  await setUpCookies();
  await setUpCache();
  extension.sendMessage({ hostnames: [] });
  await extension.awaitMessage("cacheAndCookiesRemoved");

  ok(
    Services.cookies.cookieExists(COOKIE.host, COOKIE.path, COOKIE.name, {}),
    `Cookie ${COOKIE.name}  was not removed.`
  );
  ok(
    Services.cookies.cookieExists(
      COOKIE_NET.host,
      COOKIE_NET.path,
      COOKIE_NET.name,
      {}
    ),
    `Cookie ${COOKIE_NET.name}  was not removed.`
  );
  ok(
    Services.cookies.cookieExists(
      COOKIE_ORG.host,
      COOKIE_ORG.path,
      COOKIE_ORG.name,
      {}
    ),
    `Cookie ${COOKIE_ORG.name}  was not removed.`
  );

  ok(hasCacheEntry("example.net"), "example.net cache was not removed");
  ok(hasCacheEntry("example.org"), "example.org cache was not removed");
  ok(hasCacheEntry("example.com"), "example.com cache was not removed");

  // Clear cache and cookies with both hostnames and since values.
  await setUpCache();
  await setUpCookies();
  extension.sendMessage({ hostnames: ["example.com"], since });
  await extension.awaitMessage("cacheAndCookiesRemoved");

  ok(
    Services.cookies.cookieExists(
      oldCookie.host,
      oldCookie.path,
      oldCookie.name,
      {}
    ),
    "Old cookie was not removed."
  );
  ok(
    !Services.cookies.cookieExists(COOKIE.host, COOKIE.path, COOKIE.name, {}),
    "Recent cookie was removed."
  );
  ok(
    Services.cookies.cookieExists(
      COOKIE_NET.host,
      COOKIE_NET.path,
      COOKIE_NET.name,
      {}
    ),
    "Cookie with different hostname was not removed"
  );
  ok(
    Services.cookies.cookieExists(
      COOKIE_ORG.host,
      COOKIE_ORG.path,
      COOKIE_ORG.name,
      {}
    ),
    "Cookie with different hostname was not removed"
  );

  ok(hasCacheEntry("example.net"), "example.net cache was not removed");
  ok(hasCacheEntry("example.org"), "example.org cache was not removed");
  ok(!hasCacheEntry("example.com"), "example.com cache was removed");

  // Clear cache and cookies with no since or hostnames value.
  await setUpCache();
  await setUpCookies();
  extension.sendMessage({});
  await extension.awaitMessage("cacheAndCookiesRemoved");

  ok(
    !Services.cookies.cookieExists(COOKIE.host, COOKIE.path, COOKIE.name, {}),
    `Cookie ${COOKIE.name}  was removed.`
  );
  ok(
    !Services.cookies.cookieExists(
      oldCookie.host,
      oldCookie.path,
      oldCookie.name,
      {}
    ),
    `Cookie ${oldCookie.name}  was removed.`
  );
  ok(
    !Services.cookies.cookieExists(
      COOKIE_NET.host,
      COOKIE_NET.path,
      COOKIE_NET.name,
      {}
    ),
    `Cookie ${COOKIE_NET.name}  was removed.`
  );
  ok(
    !Services.cookies.cookieExists(
      COOKIE_ORG.host,
      COOKIE_ORG.path,
      COOKIE_ORG.name,
      {}
    ),
    `Cookie ${COOKIE_ORG.name}  was removed.`
  );

  ok(!hasCacheEntry("example.net"), "example.net cache was removed");
  ok(!hasCacheEntry("example.org"), "example.org cache was removed");
  ok(!hasCacheEntry("example.com"), "example.com cache was removed");

  await extension.unload();
});
