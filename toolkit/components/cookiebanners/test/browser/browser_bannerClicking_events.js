/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(clickTestSetup);

/**
 * Triggers cookie banner clicking and tests the events dispatched.
 * @param {*} options - Test options.
 * @param {nsICookieBannerService::Modes} options.mode - The cookie banner service mode to test with.
 * @param {boolean} options.detectOnly - Whether the service should be enabled
 * in detection only mode, where it does not handle banners.
 * @param {*} options.openPageOptions - Options to overwrite for the openPageAndVerify call.
 */
async function runTest({ mode, detectOnly = false, openPageOptions = {} }) {
  if (mode == null) {
    throw new Error("Invalid cookie banner service mode.");
  }
  let initFn = () => {
    // Insert rules only if the feature is enabled.
    if (Services.cookieBanners.isEnabled) {
      insertTestClickRules();

      // Clear executed records before testing.
      Services.cookieBanners.removeAllExecutedRecords(false);
    }
  };

  let shouldHandleBanner =
    mode == Ci.nsICookieBannerService.MODE_REJECT && !detectOnly;
  let expectActorEnabled = mode != Ci.nsICookieBannerService.MODE_DISABLED;
  let testURL = openPageOptions.testURL || TEST_PAGE_A;
  let triggerFn = async () => {
    await openPageAndVerify({
      win: window,
      domain: TEST_DOMAIN_A,
      testURL,
      visible: !shouldHandleBanner,
      expected: shouldHandleBanner ? "OptOut" : "NoClick",
      keepTabOpen: true,
      expectActorEnabled,
      ...openPageOptions, // Allow test callers to override any options for this method.
    });
  };

  await runEventTest({ mode, detectOnly, initFn, triggerFn, testURL });

  // Clean up the test tab opened by openPageAndVerify.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

/**
 * Test the banner clicking events with MODE_REJECT.
 */
add_task(async function test_events_mode_reject() {
  await runTest({ mode: Ci.nsICookieBannerService.MODE_REJECT });
});

/**
 * Test the banner clicking events with detect-only mode.
 */
add_task(async function test_events_mode_detect_only() {
  await runTest({
    mode: Ci.nsICookieBannerService.MODE_REJECT,
    detectOnly: true,
  });
  await runTest({
    mode: Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
    detectOnly: true,
  });
});

/**
 * Test the banner clicking events with detect-only mode with a click rule that
 * only supports opt-in.
 */
add_task(async function test_events_mode_detect_only_opt_in_rule() {
  await runTest({
    mode: Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
    detectOnly: true,
    openPageOptions: {
      // We only have an opt-in rule for DOMAIN_B. This ensures we still fire
      // detection events for that case.
      domain: TEST_DOMAIN_B,
      testURL: TEST_PAGE_B,
      shouldHandleBanner: true,
      expected: "NoClick",
    },
  });
});

/**
 * Test the banner clicking events in disabled mode.
 */
add_task(async function test_events_mode_disabled() {
  await runTest({ mode: Ci.nsICookieBannerService.MODE_DISABLED });
});
