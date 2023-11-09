/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(clickTestSetup);

/**
 * Insert a test rule with the specified runContext.
 * @param {RunContext} - The runContext to set for the rule. See nsIClickRule
 * for documentation.
 */
function insertTestRules({ runContext }) {
  info("Clearing existing rules");
  Services.cookieBanners.resetRules(false);

  info("Inserting test rules. " + JSON.stringify({ runContext }));

  info("Add opt-out click rule for DOMAIN_A.");
  let ruleA = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleA.id = genUUID();
  ruleA.domains = [TEST_DOMAIN_A];

  ruleA.addClickRule(
    "div#banner",
    false,
    runContext,
    null,
    "button#optOut",
    "button#optIn"
  );
  Services.cookieBanners.insertRule(ruleA);
}

/**
 * Test that banner clicking only runs if the context matches the runContext
 * specified in the click rule.
 */
add_task(async function test_embedded_iframe() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  insertTestRules({ runContext: Ci.nsIClickRule.RUN_TOP });

  // Clear executed records before testing.
  Services.cookieBanners.removeAllExecutedRecords(false);

  await openIframeAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: true,
    expected: "NoClick",
  });

  Services.cookieBanners.removeAllExecutedRecords(false);

  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  Services.cookieBanners.removeAllExecutedRecords(false);

  insertTestRules({ runContext: Ci.nsIClickRule.RUN_CHILD });

  await openIframeAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  Services.cookieBanners.removeAllExecutedRecords(false);

  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: true,
    expected: "NoClick",
  });

  Services.cookieBanners.removeAllExecutedRecords(false);

  insertTestRules({ runContext: Ci.nsIClickRule.RUN_ALL });
  await openIframeAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  Services.cookieBanners.removeAllExecutedRecords(false);

  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  Services.cookieBanners.removeAllExecutedRecords(false);
});
