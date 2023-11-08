/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(clickTestSetup);

/**
 * Test that the banner clicking won't click banner if the service is disabled or in detect-only mode.
 */
add_task(async function test_cookie_banner_service_disabled() {
  for (let [serviceMode, detectOnly] of [
    [Ci.nsICookieBannerService.MODE_DISABLED, false],
    [Ci.nsICookieBannerService.MODE_DISABLED, true],
    [Ci.nsICookieBannerService.MODE_REJECT, true],
    [Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT, true],
  ]) {
    await SpecialPowers.pushPrefEnv({
      set: [
        ["cookiebanners.service.mode", serviceMode],
        ["cookiebanners.service.detectOnly", detectOnly],
        [
          "cookiebanners.service.mode.privateBrowsing",
          Ci.nsICookieBannerService.MODE_DISABLED,
        ],
      ],
    });

    // Clear the executed records before testing.
    if (serviceMode != Ci.nsICookieBannerService.MODE_DISABLED) {
      Services.cookieBanners.removeAllExecutedRecords(false);
    }

    await openPageAndVerify({
      win: window,
      domain: TEST_DOMAIN_A,
      testURL: TEST_PAGE_A,
      visible: true,
      expected: "NoClick",
      expectActorEnabled:
        serviceMode != Ci.nsICookieBannerService.MODE_DISABLED,
    });

    await SpecialPowers.popPrefEnv();
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

  // Clear the executed records before testing.
  Services.cookieBanners.removeAllExecutedRecords(false);

  info("Clearing existing rules");
  Services.cookieBanners.resetRules(false);

  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: true,
    expected: "NoClick",
  });

  // No click telemetry reported.
  await testClickResultTelemetry({});
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

  // Clear the executed records before testing.
  Services.cookieBanners.removeAllExecutedRecords(false);

  insertTestClickRules();

  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  await testClickResultTelemetry(
    { success: 1, success_dom_content_loaded: 1 },
    false
  );

  Services.cookieBanners.removeAllExecutedRecords(false);

  // No opt out rule for the example.org, the banner shouldn't be clicked.
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_B,
    testURL: TEST_PAGE_B,
    visible: true,
    expected: "NoClick",
  });

  // No matching rule means we don't record any telemetry for clicks.
  await testClickResultTelemetry({
    success: 1,
    success_dom_content_loaded: 1,
    fail: 1,
    fail_no_rule_for_mode: 1,
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

  // Clear the executed records before testing.
  Services.cookieBanners.removeAllExecutedRecords(false);

  insertTestClickRules();

  await testClickResultTelemetry({});

  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  await testClickResultTelemetry(
    {
      success: 1,
      success_dom_content_loaded: 1,
    },
    false
  );

  Services.cookieBanners.removeAllExecutedRecords(false);

  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_B,
    testURL: TEST_PAGE_B,
    visible: false,
    expected: "OptIn",
  });

  await testClickResultTelemetry({
    success: 2,
    success_dom_content_loaded: 2,
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
      ["cookiebanners.bannerClicking.timeoutAfterLoad", 10000],
    ],
  });

  // Clear the executed records before testing.
  Services.cookieBanners.removeAllExecutedRecords(false);

  insertTestClickRules();

  await testClickResultTelemetry({});

  let TEST_PAGE =
    TEST_ORIGIN_A + TEST_PATH + "file_delayed_banner.html?delay=100";
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE,
    visible: false,
    expected: "OptOut",
  });

  await testClickResultTelemetry({
    success: 1,
    success_mutation_pre_load: 1,
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

  // Clear the executed records before testing.
  Services.cookieBanners.removeAllExecutedRecords(false);

  insertTestClickRules();

  await testClickResultTelemetry({});

  await openIframeAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  await testClickResultTelemetry({
    success: 1,
    success_dom_content_loaded: 1,
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

  // Clear the executed records before testing.
  Services.cookieBanners.removeAllExecutedRecords(false);

  insertTestClickRules();

  await testClickResultTelemetry({});

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

  await testClickResultTelemetry({
    success: 1,
    success_dom_content_loaded: 1,
  });
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

  // Clear the executed records before testing.
  Services.cookieBanners.removeAllExecutedRecords(false);

  insertTestClickRules();

  await testClickResultTelemetry({});

  let pbmWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  await openPageAndVerify({
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  await testClickResultTelemetry(
    {
      success: 1,
      success_dom_content_loaded: 1,
    },
    false
  );

  Services.cookieBanners.removeAllExecutedRecords(false);

  await openPageAndVerify({
    win: pbmWindow,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: true,
    expected: "NoClick",
  });

  await testClickResultTelemetry(
    {
      success: 1,
      success_dom_content_loaded: 1,
    },
    false
  );

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

  Services.cookieBanners.removeAllExecutedRecords(false);

  await openPageAndVerify({
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: true,
    expected: "NoClick",
  });

  await testClickResultTelemetry(
    {
      success: 1,
      success_dom_content_loaded: 1,
    },
    false
  );

  Services.cookieBanners.removeAllExecutedRecords(false);

  await openPageAndVerify({
    win: pbmWindow,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  await testClickResultTelemetry(
    {
      success: 2,
      success_dom_content_loaded: 2,
    },
    false
  );

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

  Services.cookieBanners.removeAllExecutedRecords(false);

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

  await testClickResultTelemetry(
    {
      success: 3,
      success_dom_content_loaded: 3,
    },
    false
  );

  Services.cookieBanners.removeAllExecutedRecords(false);

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

  await testClickResultTelemetry({
    success: 3,
    success_dom_content_loaded: 3,
    fail: 1,
    fail_no_rule_for_mode: 1,
  });
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

  // Clear the executed records before testing.
  Services.cookieBanners.removeAllExecutedRecords(false);

  insertTestClickRules();

  await testClickResultTelemetry({});

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

  await testClickResultTelemetry({
    success: 1,
    success_dom_content_loaded: 1,
  });
});
