/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(clickTestSetup);

async function verifyDetectedCMPTelemetry(expected) {
  // Ensure we have all data from the content process.
  await Services.fog.testFlushAllChildren();

  let LABELS = [
    "trustarcbar",
    "quantcast",
    "borlabs",
    "consentmanagernet",
    "cookiebot",
    "complianz",
    "onetrust",
    "didomi",
    "sourcepoint",
  ];

  let testMetricState = doAssert => {
    for (let label of LABELS) {
      let expectedValue = expected[label] ?? null;
      if (doAssert) {
        is(
          Glean.cookieBannersCmp.detectedCmp[label].testGetValue(),
          expectedValue,
          `Counter for label '${label}' has correct state.`
        );
      } else if (
        Glean.cookieBannersCmp.detectedCmp[label].testGetValue() !==
        expectedValue
      ) {
        return false;
      }
    }

    return true;
  };

  // Wait for the labeled counter to match the expected state. Returns greedy on
  // mismatch.
  try {
    await TestUtils.waitForCondition(
      testMetricState,
      "Waiting for cookieBannersClick.result metric to match."
    );
  } finally {
    // Test again but this time with assertions and test all labels.
    testMetricState(true);
  }
}

async function verifyRatioTelemetry(expected) {
  // Ensure we have all data from the content process.
  await Services.fog.testFlushAllChildren();

  let cmpMetric = Glean.cookieBannersCmp.ratioHandledByCmpRule;

  let testMetricState = doAssert => {
    let cmpValue = cmpMetric.testGetValue();

    if (!cmpValue) {
      return false;
    }

    if (!doAssert) {
      return (
        cmpValue.numerator == expected.cmp.numerator &&
        cmpValue.denominator == expected.cmp.denominator
      );
    }

    is(
      cmpValue.numerator,
      expected.cmp.numerator,
      "The numerator of rateHandledByCmpRule is expected"
    );
    is(
      cmpValue.denominator,
      expected.cmp.denominator,
      "The denominator of rateHandledByCmpRule is expected"
    );

    return true;
  };

  // Wait for matching the expected state.
  try {
    await TestUtils.waitForCondition(
      testMetricState,
      "Waiting for metric to match."
    );
  } finally {
    // Test again but this time with assertions and test all labels.
    testMetricState(true);
  }
}

add_task(async function test_cmp_telemetry() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
      ["cookiebanners.service.enableGlobalRules", true],
      ["cookiebanners.bannerClicking.maxTriesPerSiteAndSession", 0],
    ],
  });

  info("Clearing existing rules");
  Services.cookieBanners.resetRules(false);

  info("Inserting global test rules.");

  info(
    "Add global ruleA with a predefined identifier. This rule will handle the banner."
  );
  let ruleA = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleA.id = "trustarcbar";
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

  info("Add global ruleC with another predefined identifier.");
  let ruleC = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleC.id = "quantcast";
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

  info("Open a test page and verify the telemetry");
  await openPageAndVerify({
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  await verifyRatioTelemetry({
    cmp: { numerator: 1, denominator: 1 },
  });
  await verifyDetectedCMPTelemetry({
    trustarcbar: 1,
    quantcast: 1,
  });

  info("Open the test page again and verify if the telemetry gets updated");
  await openPageAndVerify({
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  await verifyRatioTelemetry({
    cmp: { numerator: 2, denominator: 2 },
  });
  await verifyDetectedCMPTelemetry({
    trustarcbar: 2,
    quantcast: 2,
  });

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
    "button#optOut",
    "button#optIn"
  );
  Services.cookieBanners.insertRule(ruleDomain);

  info("Open the test page and now the site-specific rule will handle it");
  await openPageAndVerify({
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  info("Verify that the CMP telemetry doesn't get updated.");
  await verifyDetectedCMPTelemetry({
    trustarcbar: 2,
    quantcast: 2,
  });

  info(
    "Verify the handled ratio telemetry for site-specific rule gets updated."
  );
  await verifyRatioTelemetry({
    cmp: { numerator: 2, denominator: 3 },
  });

  // Clear Telemetry
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();
});
