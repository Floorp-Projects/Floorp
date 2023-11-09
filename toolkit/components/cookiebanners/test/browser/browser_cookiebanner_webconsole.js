/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SiteDataTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SiteDataTestUtils.sys.mjs"
);

/**
 * Registers a console listener and waits for the cookie banner handled message
 * to be logged.
 * @returns {Promise} - Promise which resolves once the message has been logged.
 */
async function waitForCookieBannerHandledConsoleMsg() {
  let msg;
  let checkFn = msg =>
    msg && msg.match(/handled a cookie banner on behalf of the user./);
  await new Promise(resolve => {
    SpecialPowers.registerConsoleListener(consoleMsg => {
      msg = consoleMsg.message;
      if (checkFn(msg)) {
        resolve();
      }
    });
  });
  SpecialPowers.postConsoleSentinel();

  ok(checkFn(msg), "Observed cookie banner handled console message.");
}

add_setup(clickTestSetup);

/**
 * Tests that when we handle a banner via clicking a message is logged to the
 * website console.
 */
add_task(async function test_banner_clicking_log_web_console() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
    ],
  });

  insertTestClickRules();

  let consoleMsgPromise = waitForCookieBannerHandledConsoleMsg();

  // Clear the executed records before testing.
  Services.cookieBanners.removeAllExecutedRecords(false);

  info("Handle the banner via click and wait for console message to appear.");
  await openPageAndVerify({
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  await consoleMsgPromise;
});

/**
 * Tests that when we handle a banner via cookie injection a message is logged
 * to the website console.
 */
add_task(async function test_cookie_injection_log_web_console() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
      ["cookiebanners.cookieInjector.enabled", true],
    ],
  });

  insertTestCookieRules();

  let consoleMsgPromise = waitForCookieBannerHandledConsoleMsg();

  // Clear the executed records before testing.
  Services.cookieBanners.removeAllExecutedRecords(false);

  info(
    "Handle the banner via cookie injection and wait for console message to appear."
  );
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_ORIGIN_A
  );
  BrowserTestUtils.removeTab(tab);

  await consoleMsgPromise;

  // Clear injected cookies.
  await SiteDataTestUtils.clear();
});
