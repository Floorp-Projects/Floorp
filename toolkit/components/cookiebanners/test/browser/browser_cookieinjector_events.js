/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SiteDataTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SiteDataTestUtils.sys.mjs"
);

add_setup(cookieInjectorTestSetup);

/**
 * Tests that we dispatch cookiebannerhandled and cookiebannerdetected events
 * for cookie injection.
 */
add_task(async function test_events() {
  let tab;

  let triggerFn = async () => {
    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_ORIGIN_A);
  };

  await runEventTest({
    mode: Ci.nsICookieBannerService.MODE_REJECT,
    initFn: insertTestCookieRules,
    triggerFn,
    testURL: `${TEST_ORIGIN_A}/`,
  });

  // Clean up the test tab opened by triggerFn.
  BrowserTestUtils.removeTab(tab);

  ok(
    SiteDataTestUtils.hasCookies(TEST_ORIGIN_A, [
      {
        key: `cookieConsent_${TEST_DOMAIN_A}_1`,
        value: "optOut1",
      },
      {
        key: `cookieConsent_${TEST_DOMAIN_A}_2`,
        value: "optOut2",
      },
    ]),
    "Should set opt-out cookies for ORIGIN_A"
  );
  ok(
    !SiteDataTestUtils.hasCookies(TEST_ORIGIN_B),
    "Should not set any cookies for ORIGIN_B"
  );
  ok(
    !SiteDataTestUtils.hasCookies(TEST_ORIGIN_C),
    "Should not set any cookies for ORIGIN_C"
  );

  await SiteDataTestUtils.clear();
});
