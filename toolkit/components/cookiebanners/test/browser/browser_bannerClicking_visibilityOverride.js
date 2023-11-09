/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// A test page that has an invisible cookie banner. This simulates sites where
// the banner is invisible whenever we test for it. See Bug 1793803.
const TEST_PAGE = TEST_ORIGIN_A + TEST_PATH + "file_banner_invisible.html";

add_setup(clickTestSetup);

/**
 * Insert a test rule with or without the skipPresenceVisibilityCheck flag.
 * @param {boolean} skipPresenceVisibilityCheck - Whether to set the flag for
 * the test rule.
 */
function insertVisibilityTestRules(skipPresenceVisibilityCheck) {
  info("Clearing existing rules");
  Services.cookieBanners.resetRules(false);

  info(
    "Inserting test rules. " + JSON.stringify({ skipPresenceVisibilityCheck })
  );

  info("Add opt-out click rule for DOMAIN_A.");
  let ruleA = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleA.id = genUUID();
  ruleA.domains = [TEST_DOMAIN_A];

  ruleA.addClickRule(
    "div#banner",
    skipPresenceVisibilityCheck,
    Ci.nsIClickRule.RUN_TOP,
    null,
    "button#optOut",
    "button#optIn"
  );
  Services.cookieBanners.insertRule(ruleA);
}

/**
 * Test that we click on an invisible banner element if
 * skipPresenceVisibilityCheck is set.
 */
add_task(async function test_clicking_with_delayed_banner() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  for (let skipPresenceVisibilityCheck of [false, true]) {
    // Clear the executed records before testing.
    Services.cookieBanners.removeAllExecutedRecords(false);

    insertVisibilityTestRules(skipPresenceVisibilityCheck);

    await testClickResultTelemetry({});

    await openPageAndVerify({
      win: window,
      domain: TEST_DOMAIN_A,
      testURL: TEST_PAGE,
      visible: false,
      expected: skipPresenceVisibilityCheck ? "OptOut" : "NoClick",
    });

    let expectedTelemetry;
    if (skipPresenceVisibilityCheck) {
      expectedTelemetry = {
        success: 1,
        success_dom_content_loaded: 1,
      };
    } else {
      expectedTelemetry = {
        fail: 1,
        fail_banner_not_visible: 1,
      };
    }
    await testClickResultTelemetry(expectedTelemetry);
  }
});
