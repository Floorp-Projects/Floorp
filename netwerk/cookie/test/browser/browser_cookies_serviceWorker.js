/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_CROSS_SITE_DOMAIN = "https://example.net/";
const TEST_CROSS_SITE_PAGE =
  TEST_CROSS_SITE_DOMAIN + TEST_PATH + "file_empty.html";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.serviceWorkers.enabled", true],
      ["dom.serviceWorkers.testing.enabled", true],
    ],
  });

  Services.cookies.removeAll();
});

function registerSW(browser) {
  return SpecialPowers.spawn(browser, [], async _ => {
    let reg = await content.navigator.serviceWorker.register(
      "serviceWorker.js"
    );

    await ContentTaskUtils.waitForCondition(() => {
      return reg.active && reg.active.state === "activated";
    }, "The service worker is activated");

    ok(
      content.navigator.serviceWorker.controller,
      "The service worker controls the document successfully."
    );
  });
}

function fetchCookiesFromSW(browser) {
  return SpecialPowers.spawn(browser, [], async _ => {
    return new content.Promise(resolve => {
      content.navigator.serviceWorker.addEventListener("message", event => {
        resolve(event.data.content);
      });

      content.navigator.serviceWorker.controller.postMessage({
        action: "fetch",
        url: `cookies.sjs`,
      });
    });
  });
}

function setCookiesFromSW(browser, cookies) {
  let setCookieQuery = "";

  for (let cookie of cookies) {
    setCookieQuery += `Set-Cookie=${cookie}&`;
  }

  return SpecialPowers.spawn(browser, [setCookieQuery], async query => {
    return new content.Promise(resolve => {
      content.navigator.serviceWorker.addEventListener("message", event => {
        resolve(event.data.content);
      });

      content.navigator.serviceWorker.controller.postMessage({
        action: "fetch",
        url: `cookies.sjs?${query}`,
      });
    });
  });
}

function unregisterSW(browser) {
  return SpecialPowers.spawn(browser, [], async _ => {
    const regs = await content.navigator.serviceWorker.getRegistrations();
    for (const reg of regs) {
      await reg.unregister();
    }
  });
}

/**
 * Verify a first-party service worker can access both SameSite=None and
 * SameSite=Lax cookies set in the first-party context.
 */
add_task(async function testCookiesWithFirstPartyServiceWorker() {
  info("Open a tab");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TOP_PAGE
  );

  info("Writing cookies to the first-party context.");
  await SpecialPowers.spawn(tab.linkedBrowser, [], async _ => {
    const cookies = [
      "foo=bar; SameSite=None; Secure",
      "fooLax=barLax; SameSite=Lax; Secure",
    ];

    let query = "";

    for (let cookie of cookies) {
      query += `Set-Cookie=${cookie}&`;
    }

    await content.fetch(`cookies.sjs?${query}`);
  });

  info("Register a service worker and trigger a fetch request to get cookies.");
  await registerSW(tab.linkedBrowser);
  let cookieStr = await fetchCookiesFromSW(tab.linkedBrowser);

  is(cookieStr, "foo=bar; fooLax=barLax", "The cookies are expected");

  info("Set cookies from the service worker.");
  await setCookiesFromSW(tab.linkedBrowser, [
    "foo=barSW; SameSite=None; Secure",
    "fooLax=barLaxSW; SameSite=Lax; Secure",
  ]);

  info("Get cookies from the service worker.");
  cookieStr = await fetchCookiesFromSW(tab.linkedBrowser);

  is(cookieStr, "foo=barSW; fooLax=barLaxSW", "The cookies are expected");

  await unregisterSW(tab.linkedBrowser);
  BrowserTestUtils.removeTab(tab);
  Services.cookies.removeAll();
});

/**
 * Verify a cross-site service worker can only access cookies set in the
 * same cross-site context.
 */
add_task(async function testCookiesWithCrossSiteServiceWorker() {
  // Disable blocking third-party cookies.
  await SpecialPowers.pushPrefEnv({
    set: [["network.cookie.cookieBehavior.optInPartitioning", false]],
  });

  info("Open a cross-site tab");
  let crossSiteTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_CROSS_SITE_PAGE
  );

  info("Writing cookies to the cross site in the first-party context.");
  await SpecialPowers.spawn(crossSiteTab.linkedBrowser, [], async _ => {
    const cookies = [
      "foo=bar; SameSite=None; Secure",
      "fooLax=barLax; SameSite=Lax; Secure",
    ];

    let query = "";

    for (let cookie of cookies) {
      query += `Set-Cookie=${cookie}&`;
    }

    await content.fetch(`cookies.sjs?${query}`);
  });

  info("Open a tab");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TOP_PAGE
  );

  info("Load a cross-site iframe");
  let crossSiteBc = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [TEST_CROSS_SITE_PAGE],
    async url => {
      let ifr = content.document.createElement("iframe");

      await new content.Promise(resolve => {
        ifr.onload = resolve;
        content.document.body.appendChild(ifr);
        ifr.src = url;
      });

      return ifr.browsingContext;
    }
  );

  info("Write cookies in the cross-site iframe");
  await SpecialPowers.spawn(crossSiteBc, [], async _ => {
    const cookies = [
      "foo=crossBar; SameSite=None; Secure",
      "fooLax=crossBarLax; SameSite=Lax; Secure",
    ];

    let query = "";

    for (let cookie of cookies) {
      query += `Set-Cookie=${cookie}&`;
    }

    await content.fetch(`cookies.sjs?${query}`);
  });

  info(
    "Register a service worker and trigger a fetch request to get cookies in cross-site context."
  );
  await registerSW(crossSiteBc);
  let cookieStr = await fetchCookiesFromSW(crossSiteBc);

  is(
    cookieStr,
    "foo=crossBar",
    "Only the SameSite=None cookie set in the third-party iframe is available."
  );

  info("Set cookies from the third-party service worker.");
  await setCookiesFromSW(crossSiteBc, [
    "foo=crossBarSW; SameSite=None; Secure",
    "fooLax=crossBarLaxSW; SameSite=Lax; Secure",
  ]);

  info("Get cookies from the third-party service worker.");
  cookieStr = await fetchCookiesFromSW(crossSiteBc);

  is(
    cookieStr,
    "foo=crossBarSW",
    "Only the SameSite=None cookie set in the third-party service worker is available."
  );

  await unregisterSW(crossSiteBc);
  BrowserTestUtils.removeTab(crossSiteTab);
  BrowserTestUtils.removeTab(tab);
  Services.cookies.removeAll();
});

/**
 * Verify a cross-site service worker can only access partitioned cookies set in
 * the same cross-site context if third-party cookies are blocked.
 */
add_task(async function testPartitionedCookiesWithCrossSiteServiceWorker() {
  // Enable blocking third-party cookies.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.cookie.cookieBehavior.optInPartitioning", true],
      ["network.cookie.CHIPS.enabled", true],
    ],
  });

  info("Open a tab");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TOP_PAGE
  );

  info("Load a cross-site iframe");
  let crossSiteBc = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [TEST_CROSS_SITE_PAGE],
    async url => {
      let ifr = content.document.createElement("iframe");

      await new content.Promise(resolve => {
        ifr.onload = resolve;
        content.document.body.appendChild(ifr);
        ifr.src = url;
      });

      return ifr.browsingContext;
    }
  );

  info("Write cookies in the cross-site iframe");
  await SpecialPowers.spawn(crossSiteBc, [], async _ => {
    const cookies = [
      "foo=crossBar; SameSite=None; Secure",
      "fooLax=crossBarLax; SameSite=Lax; Secure",
      "fooPartitioned=crossBar; SameSite=None; Secure; Partitioned;",
      "fooLaxPartitioned=crossBarLax; SameSite=Lax; Secure; Partitioned;",
    ];

    let query = "";

    for (let cookie of cookies) {
      query += `Set-Cookie=${cookie}&`;
    }

    await content.fetch(`cookies.sjs?${query}`);
  });

  info(
    "Register a service worker and trigger a fetch request to get cookies in cross-site context."
  );
  await registerSW(crossSiteBc);
  let cookieStr = await fetchCookiesFromSW(crossSiteBc);

  is(
    cookieStr,
    "fooPartitioned=crossBar",
    "Only the SameSite=None partitioned cookie set in the third-party iframe is available."
  );

  info("Set cookies from the third-party service worker.");
  await setCookiesFromSW(crossSiteBc, [
    "foo=crossBarSW; SameSite=None; Secure",
    "fooLax=crossBarLaxSW; SameSite=Lax; Secure",
    "fooPartitioned=crossBarSW; SameSite=None; Secure; Partitioned;",
    "fooLaxPartitioned=crossBarLaxSW; SameSite=Lax; Secure; Partitioned;",
  ]);

  info("Get cookies from the third-party service worker.");
  cookieStr = await fetchCookiesFromSW(crossSiteBc);

  is(
    cookieStr,
    "fooPartitioned=crossBarSW",
    "Only the SameSite=None partitioned cookie set in the third-party service worker is available."
  );

  await unregisterSW(crossSiteBc);
  BrowserTestUtils.removeTab(tab);
  Services.cookies.removeAll();
});

/**
 * Verify a ABA service worker can only access cookies set in the ABA context.
 */
add_task(async function testCookiesWithABAServiceWorker() {
  // Disable blocking third-party cookies.
  await SpecialPowers.pushPrefEnv({
    set: [["network.cookie.cookieBehavior.optInPartitioning", false]],
  });

  info("Open a tab");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TOP_PAGE
  );

  info("Writing cookies to the first-party context.");
  await SpecialPowers.spawn(tab.linkedBrowser, [], async _ => {
    const cookies = [
      "foo=bar; SameSite=None; Secure",
      "fooLax=barLax; SameSite=Lax; Secure",
    ];

    let query = "";

    for (let cookie of cookies) {
      query += `Set-Cookie=${cookie}&`;
    }

    await content.fetch(`cookies.sjs?${query}`);
  });

  info("Load a ABA iframe");
  let crossSiteBc = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [TEST_CROSS_SITE_PAGE],
    async url => {
      let ifr = content.document.createElement("iframe");

      await new content.Promise(resolve => {
        ifr.onload = resolve;
        content.document.body.appendChild(ifr);
        ifr.src = url;
      });

      return ifr.browsingContext;
    }
  );

  let ABABc = await SpecialPowers.spawn(
    crossSiteBc,
    [TEST_TOP_PAGE],
    async url => {
      let ifr = content.document.createElement("iframe");

      await new content.Promise(resolve => {
        ifr.onload = resolve;
        content.document.body.appendChild(ifr);
        ifr.src = url;
      });

      return ifr.browsingContext;
    }
  );

  info(
    "Register a service worker and trigger a fetch request to get cookies in the ABA context."
  );

  await registerSW(ABABc);
  let cookieStr = await fetchCookiesFromSW(ABABc);
  is(cookieStr, "", "No cookie should be available in ABA context.");

  info("Set cookies in the ABA iframe");
  await SpecialPowers.spawn(ABABc, [], async _ => {
    const cookies = [
      "fooABA=barABA; SameSite=None; Secure",
      "fooABALax=BarABALax; SameSite=Lax; Secure",
    ];

    let query = "";

    for (let cookie of cookies) {
      query += `Set-Cookie=${cookie}&`;
    }

    await content.fetch(`cookies.sjs?${query}`);
  });

  info("Get cookies in the ABA service worker.");
  cookieStr = await fetchCookiesFromSW(ABABc);

  is(
    cookieStr,
    "fooABA=barABA",
    "Only the SameSite=None cookie set in ABA iframe is available."
  );

  info("Set cookies from the service worker in ABA context");
  await setCookiesFromSW(ABABc, [
    "fooABA=barABASW; SameSite=None; Secure",
    "fooABALax=BarABALaxSW; SameSite=Lax; Secure",
  ]);

  info("Get cookies from the service worker in the ABA context.");
  cookieStr = await fetchCookiesFromSW(ABABc);

  is(
    cookieStr,
    "fooABA=barABASW",
    "Only the SameSite=None cookie set in ABA service worker is available."
  );

  await unregisterSW(ABABc);
  BrowserTestUtils.removeTab(tab);
  Services.cookies.removeAll();
});

/**
 * Verify a ABA service worker can only access partitioned cookies set in the
 * ABA context if third-party cookies are blocked.
 */
add_task(async function testCookiesWithABAServiceWorker() {
  // Disable blocking third-party cookies.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.cookie.cookieBehavior.optInPartitioning", true],
      ["network.cookie.CHIPS.enabled", true],
    ],
  });

  info("Open a tab");
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TOP_PAGE
  );

  info("Load a ABA iframe");
  let crossSiteBc = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [TEST_CROSS_SITE_PAGE],
    async url => {
      let ifr = content.document.createElement("iframe");

      await new content.Promise(resolve => {
        ifr.onload = resolve;
        content.document.body.appendChild(ifr);
        ifr.src = url;
      });

      return ifr.browsingContext;
    }
  );

  let ABABc = await SpecialPowers.spawn(
    crossSiteBc,
    [TEST_TOP_PAGE],
    async url => {
      let ifr = content.document.createElement("iframe");

      await new content.Promise(resolve => {
        ifr.onload = resolve;
        content.document.body.appendChild(ifr);
        ifr.src = url;
      });

      return ifr.browsingContext;
    }
  );

  info("Set cookies in the ABA iframe");
  await SpecialPowers.spawn(ABABc, [], async _ => {
    const cookies = [
      "fooABA=barABA; SameSite=None; Secure",
      "fooABALax=BarABALax; SameSite=Lax; Secure",
      "fooABAPartitioned=barABA; SameSite=None; Secure; Partitioned;",
      "fooABALaxPartitioned=BarABALax; SameSite=Lax; Secure; Partitioned;",
    ];

    let query = "";

    for (let cookie of cookies) {
      query += `Set-Cookie=${cookie}&`;
    }

    await content.fetch(`cookies.sjs?${query}`);
  });

  info(
    "Register a service worker and trigger a fetch request to get cookies in the ABA context."
  );

  await registerSW(ABABc);

  info("Get cookies in the ABA service worker.");
  let cookieStr = await fetchCookiesFromSW(ABABc);

  is(
    cookieStr,
    "fooABAPartitioned=barABA",
    "Only the SameSite=None partitioned cookie set in ABA iframe is available."
  );

  info("Set cookies from the service worker in ABA context");
  await setCookiesFromSW(ABABc, [
    "fooABA=barABASW; SameSite=None; Secure",
    "fooABALax=BarABALaxSW; SameSite=Lax; Secure",
    "fooABAPartitioned=barABASW; SameSite=None; Secure; Partitioned;",
    "fooABALaxPartitioned=BarABALaxSW; SameSite=Lax; Secure; Partitioned;",
  ]);

  info("Get cookies from the service worker in the ABA context.");
  cookieStr = await fetchCookiesFromSW(ABABc);

  is(
    cookieStr,
    "fooABAPartitioned=barABASW",
    "Only the SameSite=None partitioned cookie set in ABA service worker is available."
  );

  await unregisterSW(ABABc);
  BrowserTestUtils.removeTab(tab);
  Services.cookies.removeAll();
});
