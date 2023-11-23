/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(clickTestSetup);

/**
 * Test the banner clicking with global rules and MODE_REJECT.
 */
add_task(async function test_clicking_global_rules() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
      ["cookiebanners.service.enableGlobalRules", true],
    ],
  });

  // Clear the executed records before testing.
  Services.cookieBanners.removeAllExecutedRecords(false);

  info("Clearing existing rules");
  Services.cookieBanners.resetRules(false);

  info("Inserting global test rules.");

  info(
    "Add global ruleA which targets an existing banner (presence) with existing buttons. This rule should handle the banner."
  );
  let ruleA = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleA.id = genUUID();
  ruleA.domains = [];
  ruleA.addClickRule(
    "div#banner",
    false,
    Ci.nsIClickRule.RUN_TOP,
    null,
    "button#optOut",
    "button#optIn"
  );
  Services.cookieBanners.insertRule(ruleA);

  info(
    "Add global ruleC which targets an existing banner (presence) but non-existing buttons."
  );
  let ruleC = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleC.id = genUUID();
  ruleC.domains = [];
  ruleC.addClickRule(
    "div#banner",
    false,
    Ci.nsIClickRule.RUN_TOP,
    null,
    "button#nonExistingOptOut",
    "button#nonExistingOptIn"
  );
  Services.cookieBanners.insertRule(ruleC);

  info("Add global ruleD which targets a non-existing banner (presence).");
  let ruleD = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleD.id = genUUID();
  ruleD.domains = [];
  ruleD.addClickRule(
    "div#nonExistingBanner",
    false,
    Ci.nsIClickRule.RUN_TOP,
    null,
    null,
    "button#optIn"
  );
  Services.cookieBanners.insertRule(ruleD);

  await testCMPResultTelemetry({});

  info("The global rule ruleA should handle both test pages with div#banner.");
  await openPageAndVerify({
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  await testCMPResultTelemetry(
    {
      success: 1,
      success_dom_content_loaded: 1,
    },
    false
  );

  Services.cookieBanners.removeAllExecutedRecords(false);

  await openPageAndVerify({
    domain: TEST_DOMAIN_B,
    testURL: TEST_PAGE_B,
    visible: false,
    expected: "OptOut",
  });

  await testCMPResultTelemetry(
    {
      success: 2,
      success_dom_content_loaded: 2,
    },
    false
  );

  Services.cookieBanners.removeAllExecutedRecords(false);

  info("No global rule should handle TEST_PAGE_C with div#bannerB.");
  await openPageAndVerify({
    domain: TEST_DOMAIN_C,
    testURL: TEST_PAGE_C,
    visible: true,
    expected: "NoClick",
    bannerId: "bannerB",
  });

  await testCMPResultTelemetry(
    {
      success: 2,
      success_dom_content_loaded: 2,
      fail: 1,
      fail_banner_not_found: 1,
    },
    false
  );

  await SpecialPowers.pushPrefEnv({
    set: [["cookiebanners.bannerClicking.timeoutAfterLoad", 10000]],
  });

  // Clear the executed records before testing.
  Services.cookieBanners.removeAllExecutedRecords(false);

  info("Test delayed banner handling with global rules.");
  let TEST_PAGE =
    TEST_ORIGIN_A + TEST_PATH + "file_delayed_banner.html?delay=100";
  await openPageAndVerify({
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE,
    visible: false,
    expected: "OptOut",
  });

  await SpecialPowers.popPrefEnv();

  await testCMPResultTelemetry({
    success: 3,
    success_dom_content_loaded: 2,
    fail: 1,
    fail_banner_not_found: 1,
    success_mutation_pre_load: 1,
  });
});

/**
 * Test that domain-specific rules take precedence over global rules.
 */
add_task(async function test_clicking_global_rules_precedence() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
      ["cookiebanners.service.enableGlobalRules", true],
    ],
  });

  // Clear the executed records before testing.
  Services.cookieBanners.removeAllExecutedRecords(false);

  info("Clearing existing rules");
  Services.cookieBanners.resetRules(false);

  info("Inserting global test rules.");

  info(
    "Add global ruleA which targets an existing banner (presence) with existing buttons."
  );
  let ruleGlobal = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleGlobal.id = genUUID();
  ruleGlobal.domains = [];
  ruleGlobal.addClickRule(
    "div#banner",
    false,
    Ci.nsIClickRule.RUN_TOP,
    null,
    "button#optOut",
    null
  );
  Services.cookieBanners.insertRule(ruleGlobal);

  info("Add domain specific rule which also targets the existing banner.");
  let ruleDomain = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleDomain.id = genUUID();
  ruleDomain.domains = [TEST_DOMAIN_A];
  ruleDomain.addClickRule(
    "div#banner",
    false,
    Ci.nsIClickRule.RUN_TOP,
    null,
    null,
    "button#optIn"
  );
  Services.cookieBanners.insertRule(ruleDomain);

  await testCMPResultTelemetry({});

  info("Test that the domain-specific rule applies, not the global one.");
  await openPageAndVerify({
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    // Because of the way the rules are setup OptOut would mean the global rule
    // applies, opt-in means the domain specific rule applies.
    expected: "OptIn",
  });

  // Ensure we don't accidentally collect CMP result telemetry when
  // domain-specific rule applies.
  await testCMPResultTelemetry({});
});
