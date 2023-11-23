/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SiteDataTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SiteDataTestUtils.sys.mjs"
);

add_setup(async function () {
  await cookieInjectorTestSetup();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.bannerClicking.logLevel", "Debug"],
      ["cookiebanners.bannerClicking.testing", true],
    ],
  });
});

add_task(async function testCookieInjectionFailTelemetry() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  info("Clearing existing rules");
  Services.cookieBanners.resetRules(false);

  info("Creating a rule contains both cookie and click rules.");
  let rule = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  rule.id = genUUID();
  rule.domains = [TEST_DOMAIN_A];

  rule.addCookie(
    true,
    `cookieConsent_${TEST_DOMAIN_A}_1`,
    "optOut1",
    null, // empty host to fall back to .<domain>
    "/",
    3600,
    "",
    false,
    false,
    false,
    0,
    0
  );

  rule.addClickRule(
    "div#banner",
    false,
    Ci.nsIClickRule.RUN_ALL,
    null,
    "button#optOut",
    "button#optIn"
  );
  Services.cookieBanners.insertRule(rule);

  Services.cookieBanners.removeAllExecutedRecords(false);

  // Open the test page with the banner to trigger the cookie injection failure.
  let promise = promiseBannerClickingFinish(TEST_DOMAIN_A);
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE_A);
  await promise;

  await Services.fog.testFlushAllChildren();

  BrowserTestUtils.removeTab(tab);

  await TestUtils.waitForCondition(_ => {
    return Glean.cookieBanners.cookieInjectionFail.testGetValue() != null;
  }, "Waiting for metric to be ready.");

  is(
    Glean.cookieBanners.cookieInjectionFail.testGetValue(),
    1,
    "Counter for cookie_injection_fail has correct state"
  );

  // Clear Telemetry
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  await SiteDataTestUtils.clear();
});
