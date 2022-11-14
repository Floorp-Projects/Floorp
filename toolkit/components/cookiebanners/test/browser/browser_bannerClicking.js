/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(clickTestSetup);

/**
 * Test that the banner clicking won't click banner if the service is disabled or in detect-only mode.
 */
add_task(async function test_cookie_banner_service_disabled() {
  for (let mode of [
    Ci.nsICookieBannerService.MODE_DISABLED,
    Ci.nsICookieBannerService.MODE_DETECT_ONLY,
  ]) {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["cookiebanners.service.mode", mode],
        [
          "cookiebanners.service.mode.privateBrowsing",
          Ci.nsICookieBannerService.MODE_DISABLED,
        ],
      ],
    });

    await openPageAndVerify({
      win: window,
      domain: TEST_DOMAIN_A,
      testURL: TEST_PAGE_A,
      visible: true,
      expected: "NoClick",
    });
  }
});

/**
 * Test that the banner clicking won't click banner if there is no rule.
 */
add_task(async function test_no_rules() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  info("Clearing existing rules");
  Services.cookieBanners.resetRules(false);

  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: true,
    expected: "NoClick",
  });
});

/**
 * Test the banner clicking with MODE_REJECT.
 */
add_task(async function test_clicking_mode_reject() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  insertTestClickRules();

  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  // No opt out rule for the example.org, the banner shouldn't be clicked.
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_B,
    testURL: TEST_PAGE_B,
    visible: true,
    expected: "NoClick",
  });
});

/**
 * Test the banner clicking with MODE_REJECT_OR_ACCEPT.
 */
add_task(async function test_clicking_mode_reject_or_accept() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
    ],
  });

  insertTestClickRules();

  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_B,
    testURL: TEST_PAGE_B,
    visible: false,
    expected: "OptIn",
  });
});

/**
 * Test the banner clicking with the case where the banner is added after
 * page loads and with a short amount of delay.
 */
add_task(async function test_clicking_with_delayed_banner() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  insertTestClickRules();

  let TEST_PAGE =
    TEST_ORIGIN_A + TEST_PATH + "file_delayed_banner.html?delay=100";
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE,
    visible: false,
    expected: "OptOut",
  });
});

/**
 * Test that the banner clicking in an iframe.
 */
add_task(async function test_embedded_iframe() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  insertTestClickRules();

  await openIframeAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });
});

/**
 * Test banner clicking with the private browsing window.
 */
add_task(async function test_pbm() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_REJECT,
      ],
    ],
  });

  insertTestClickRules();

  let pbmWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  await openPageAndVerify({
    win: pbmWindow,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  await BrowserTestUtils.closeWindow(pbmWindow);
});

/**
 * Tests service mode pref combinations for normal and private browsing.
 */
add_task(async function test_pref_pbm_pref() {
  info("Enable in normal browsing but disable in private browsing.");
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_DISABLED,
      ],
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  insertTestClickRules();

  let pbmWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  await openPageAndVerify({
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  await openPageAndVerify({
    win: pbmWindow,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: true,
    expected: "NoClick",
  });

  info("Disable in normal browsing but enable in private browsing.");
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_REJECT,
      ],
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_DISABLED],
    ],
  });

  await openPageAndVerify({
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: true,
    expected: "NoClick",
  });

  await openPageAndVerify({
    win: pbmWindow,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  info(
    "Set normal browsing to REJECT_OR_ACCEPT and private browsing to REJECT."
  );
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_REJECT,
      ],
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
    ],
  });

  info(
    "The normal browsing window accepts the banner according to the opt-in rule."
  );
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_B,
    testURL: TEST_PAGE_B,
    visible: false,
    expected: "OptIn",
  });

  info(
    "The private browsing window should not perform any click, because there is only an opt-in rule."
  );
  await openPageAndVerify({
    win: pbmWindow,
    domain: TEST_DOMAIN_B,
    testURL: TEST_PAGE_B,
    visible: true,
    expected: "NoClick",
  });

  await BrowserTestUtils.closeWindow(pbmWindow);
});

/**
 * Test that the banner clicking in an iframe with the private browsing window.
 */
add_task(async function test_embedded_iframe_pbm() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_REJECT,
      ],
    ],
  });

  insertTestClickRules();

  let pbmWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  await openIframeAndVerify({
    win: pbmWindow,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  await BrowserTestUtils.closeWindow(pbmWindow);
});
