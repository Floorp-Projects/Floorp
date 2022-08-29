/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SiteDataTestUtils } = ChromeUtils.import(
  "resource://testing-common/SiteDataTestUtils.jsm"
);

const DOMAIN_A = "example.com";
const DOMAIN_B = "example.org";

const ORIGIN_A = "https://" + DOMAIN_A;
const ORIGIN_A_SUB = `https://test1.${DOMAIN_A}`;
const ORIGIN_B = "https://" + DOMAIN_B;

const TEST_COOKIE_HEADER_URL =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "testCookieHeader.sjs";

/**
 * Inserts cookie injection test rules for DOMAIN_A and DOMAIN_B.
 */
function insertTestRules() {
  info("Clearing existing rules");
  Services.cookieBanners.resetRules(false);

  info("Inserting test rules.");

  let ruleA = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleA.domain = DOMAIN_A;

  Services.cookieBanners.insertRule(ruleA);
  ruleA.addCookie(
    true,
    `cookieConsent_${DOMAIN_A}_1`,
    "optOut1",
    "." + DOMAIN_A,
    "/",
    3600,
    "",
    false,
    false,
    false,
    0,
    0
  );
  ruleA.addCookie(
    true,
    `cookieConsent_${DOMAIN_A}_2`,
    "optOut2",
    DOMAIN_A,
    "/",
    3600,
    "",
    false,
    false,
    false,
    0,
    0
  );

  // An opt-in cookie rule for DOMAIN_B.
  let ruleB = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleB.domain = DOMAIN_B;

  Services.cookieBanners.insertRule(ruleB);
  ruleB.addCookie(
    false,
    `cookieConsent_${DOMAIN_B}_1`,
    "optIn1",
    DOMAIN_B,
    "/",
    3600,
    "UNSET",
    false,
    false,
    true,
    0,
    0
  );
}
/**
 * Tests that the test domains have no cookies set.
 */
function assertNoCookies() {
  ok(
    !SiteDataTestUtils.hasCookies(ORIGIN_A),
    "Should not set any cookies for ORIGIN_A"
  );
  ok(
    !SiteDataTestUtils.hasCookies(ORIGIN_B),
    "Should not set any cookies for ORIGIN_B"
  );
}

/**
 * Loads a list of urls consecutively from the same tab.
 * @param {string[]} urls - List of urls to load.
 */
async function visitTestSites(urls = [ORIGIN_A, ORIGIN_B]) {
  let tab = BrowserTestUtils.addTab(gBrowser, "about:blank");

  for (let url of urls) {
    await BrowserTestUtils.loadURI(tab.linkedBrowser, url);
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  }

  BrowserTestUtils.removeTab(tab);
}

add_setup(async function() {
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("cookiebanners.service.mode");
    if (
      Services.prefs.getIntPref("cookiebanners.service.mode") !=
      Ci.nsICookieBannerService.MODE_DISABLED
    ) {
      // Restore original rules.
      Services.cookieBanners.resetRules(true);
    }
  });
});

/**
 * Tests that no cookies are set if the cookie injection component is disabled
 * by pref, but the cookie banner service is enabled.
 */
add_task(async function test_cookie_injector_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
      ["cookiebanners.cookieInjector.enabled", false],
    ],
  });

  insertTestRules();

  await visitTestSites();
  assertNoCookies();

  await SiteDataTestUtils.clear();
});

/**
 * Tests that no cookies are set if the cookie injection component is enabled
 * by pref, but the cookie banner service is disabled.
 */
add_task(async function test_cookie_banner_service_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_DISABLED],
      ["cookiebanners.cookieInjector.enabled", true],
    ],
  });

  await visitTestSites();
  assertNoCookies();

  await SiteDataTestUtils.clear();
});

/**
 * Tests that we don't inject cookies if there are no matching rules.
 */
add_task(async function test_no_rules() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
      ["cookiebanners.cookieInjector.enabled", true],
    ],
  });

  info("Clearing existing rules");
  Services.cookieBanners.resetRules(false);

  await visitTestSites();
  assertNoCookies();

  await SiteDataTestUtils.clear();
});

/**
 * Tests that inject the correct cookies for matching rules and MODE_REJECT.
 */
add_task(async function test_mode_reject() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
      ["cookiebanners.cookieInjector.enabled", true],
    ],
  });

  insertTestRules();

  await visitTestSites();

  ok(
    SiteDataTestUtils.hasCookies(ORIGIN_A, [
      {
        key: `cookieConsent_${DOMAIN_A}_1`,
        value: "optOut1",
      },
      {
        key: `cookieConsent_${DOMAIN_A}_2`,
        value: "optOut2",
      },
    ]),
    "Should set opt-out cookies for ORIGIN_A"
  );
  ok(
    !SiteDataTestUtils.hasCookies(ORIGIN_B),
    "Should not set any cookies for ORIGIN_B"
  );

  await SiteDataTestUtils.clear();
});

/**
 * Tests that inject the correct cookies for matching rules and
 * MODE_REJECT_OR_ACCEPT.
 */
add_task(async function test_mode_reject_or_accept() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
      ["cookiebanners.cookieInjector.enabled", true],
    ],
  });

  insertTestRules();

  await visitTestSites();

  ok(
    SiteDataTestUtils.hasCookies(ORIGIN_A, [
      {
        key: `cookieConsent_${DOMAIN_A}_1`,
        value: "optOut1",
      },
      {
        key: `cookieConsent_${DOMAIN_A}_2`,
        value: "optOut2",
      },
    ]),
    "Should set opt-out cookies for ORIGIN_A"
  );
  ok(
    SiteDataTestUtils.hasCookies(ORIGIN_B, [
      {
        key: `cookieConsent_${DOMAIN_B}_1`,
        value: "optIn1",
      },
    ]),
    "Should set opt-in cookies for ORIGIN_B"
  );

  await SiteDataTestUtils.clear();
});

/**
 * Test that embedded third-parties do not trigger cookie injection.
 */
add_task(async function test_embedded_third_party() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
      ["cookiebanners.cookieInjector.enabled", true],
    ],
  });

  insertTestRules();

  info("Loading example.com with an iframe for example.org.");
  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    await SpecialPowers.spawn(browser, [], async () => {
      let iframe = content.document.createElement("iframe");
      iframe.src = "https://example.org";
      content.document.body.appendChild(iframe);
      await ContentTaskUtils.waitForEvent(iframe, "load");
    });
  });

  ok(
    SiteDataTestUtils.hasCookies(ORIGIN_A, [
      {
        key: `cookieConsent_${DOMAIN_A}_1`,
        value: "optOut1",
      },
      {
        key: `cookieConsent_${DOMAIN_A}_2`,
        value: "optOut2",
      },
    ]),
    "Should set opt-out cookies for top-level ORIGIN_A"
  );
  ok(
    !SiteDataTestUtils.hasCookies(ORIGIN_B),
    "Should not set any cookies for embedded ORIGIN_B"
  );

  await SiteDataTestUtils.clear();
});

/**
 * Test that the injected cookies are present in the cookie header for the
 * initial top level document request.
 */
add_task(async function test_cookie_header_and_document() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
      ["cookiebanners.cookieInjector.enabled", true],
    ],
  });
  insertTestRules();

  await BrowserTestUtils.withNewTab(TEST_COOKIE_HEADER_URL, async browser => {
    await SpecialPowers.spawn(browser, [], async () => {
      const EXPECTED_COOKIE_STR =
        "cookieConsent_example.com_1=optOut1; cookieConsent_example.com_2=optOut2";
      is(
        content.document.body.innerText,
        EXPECTED_COOKIE_STR,
        "Sent the correct cookie header."
      );
      is(
        content.document.cookie,
        EXPECTED_COOKIE_STR,
        "document.cookie has the correct cookie string."
      );
    });
  });

  await SiteDataTestUtils.clear();
});

/**
 * Test that cookies get properly injected for private browsing mode.
 */
add_task(async function test_pbm() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
      ["cookiebanners.cookieInjector.enabled", true],
    ],
  });
  insertTestRules();

  let pbmWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  let tab = BrowserTestUtils.addTab(pbmWindow.gBrowser, "about:blank");
  await BrowserTestUtils.loadURI(tab.linkedBrowser, ORIGIN_A);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  ok(
    SiteDataTestUtils.hasCookies(
      tab.linkedBrowser.contentPrincipal.origin,
      [
        {
          key: `cookieConsent_${DOMAIN_A}_1`,
          value: "optOut1",
        },
        {
          key: `cookieConsent_${DOMAIN_A}_2`,
          value: "optOut2",
        },
      ],
      true
    ),
    "Should set opt-out cookies for top-level ORIGIN_A in private browsing."
  );

  ok(
    !SiteDataTestUtils.hasCookies(ORIGIN_A),
    "Should not set any cookies for ORIGIN_A without PBM origin attribute."
  );
  ok(
    !SiteDataTestUtils.hasCookies(ORIGIN_B),
    "Should not set any cookies for ORIGIN_B"
  );

  await BrowserTestUtils.closeWindow(pbmWindow);
  await SiteDataTestUtils.clear();
});

/**
 * Test that cookies get properly injected for container tabs.
 */
add_task(async function test_container_tab() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
      ["cookiebanners.cookieInjector.enabled", true],
    ],
  });
  insertTestRules();

  info("Loading ORIGIN_B in a container tab.");
  let tab = BrowserTestUtils.addTab(gBrowser, ORIGIN_B, {
    userContextId: 1,
  });
  await BrowserTestUtils.loadURI(tab.linkedBrowser, ORIGIN_B);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  ok(
    !SiteDataTestUtils.hasCookies(ORIGIN_A),
    "Should not set any cookies for ORIGIN_A"
  );
  ok(
    SiteDataTestUtils.hasCookies(tab.linkedBrowser.contentPrincipal.origin, [
      {
        key: `cookieConsent_${DOMAIN_B}_1`,
        value: "optIn1",
      },
    ]),
    "Should set opt-out cookies for top-level ORIGIN_B in user context 1."
  );
  ok(
    !SiteDataTestUtils.hasCookies(ORIGIN_B),
    "Should not set any cookies for ORIGIN_B in default user context"
  );

  BrowserTestUtils.removeTab(tab);
  await SiteDataTestUtils.clear();
});

/**
 * Test that if there is already a cookie with the given key, we don't overwrite
 * it. If the rule sets the unsetValue field, this cookie may be overwritten if
 * the value matches.
 */
add_task(async function test_no_overwrite() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
      ["cookiebanners.cookieInjector.enabled", true],
    ],
  });

  insertTestRules();

  info("Pre-setting a cookie that should not be overwritten.");
  SiteDataTestUtils.addToCookies({
    origin: ORIGIN_A,
    host: `.${DOMAIN_A}`,
    path: "/",
    name: `cookieConsent_${DOMAIN_A}_1`,
    value: "KEEPME",
  });

  info("Pre-setting a cookie that should be overwritten, based on its value");
  SiteDataTestUtils.addToCookies({
    origin: ORIGIN_B,
    host: `.${DOMAIN_B}`,
    path: "/",
    name: `cookieConsent_${DOMAIN_B}_1`,
    value: "UNSET",
  });

  await visitTestSites();

  ok(
    SiteDataTestUtils.hasCookies(ORIGIN_A, [
      {
        key: `cookieConsent_${DOMAIN_A}_1`,
        value: "KEEPME",
      },
      {
        key: `cookieConsent_${DOMAIN_A}_2`,
        value: "optOut2",
      },
    ]),
    "Should retain pre-set opt-in cookies for ORIGIN_A, but write new secondary cookie from rules."
  );
  ok(
    SiteDataTestUtils.hasCookies(ORIGIN_B, [
      {
        key: `cookieConsent_${DOMAIN_B}_1`,
        value: "optIn1",
      },
    ]),
    "Should have overwritten cookie for ORIGIN_B, based on its value and the unsetValue rule property."
  );

  await SiteDataTestUtils.clear();
});

/**
 * Tests that cookies are injected for the base domain when visiting a
 * subdomain.
 */
add_task(async function test_subdomain() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
      ["cookiebanners.cookieInjector.enabled", true],
    ],
  });

  insertTestRules();

  await visitTestSites([ORIGIN_A_SUB, ORIGIN_B]);

  ok(
    SiteDataTestUtils.hasCookies(ORIGIN_A, [
      {
        key: `cookieConsent_${DOMAIN_A}_1`,
        value: "optOut1",
      },
      {
        key: `cookieConsent_${DOMAIN_A}_2`,
        value: "optOut2",
      },
    ]),
    "Should set opt-out cookies for ORIGIN_A when visiting subdomain."
  );
  ok(
    !SiteDataTestUtils.hasCookies(ORIGIN_B),
    "Should not set any cookies for ORIGIN_B"
  );

  await SiteDataTestUtils.clear();
});
