/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SiteDataTestUtils } = ChromeUtils.import(
  "resource://testing-common/SiteDataTestUtils.jsm"
);

const {
  MODE_DISABLED,
  MODE_REJECT,
  MODE_REJECT_OR_ACCEPT,
  MODE_DETECT_ONLY,
  MODE_UNSET,
} = Ci.nsICookieBannerService;

const TEST_MODES = [
  MODE_DISABLED,
  MODE_REJECT,
  MODE_REJECT_OR_ACCEPT,
  MODE_DETECT_ONLY,
  MODE_UNSET, // Should be recorded as invalid.
  99, // Invalid
  -1, // Invalid
];

function convertModeToTelemetryString(mode) {
  switch (mode) {
    case MODE_DISABLED:
      return "disabled";
    case MODE_REJECT:
      return "reject";
    case MODE_REJECT_OR_ACCEPT:
      return "reject_or_accept";
    case MODE_DETECT_ONLY:
      return "detect_only";
  }

  return "invalid";
}

/**
 * A helper function to verify the cookie rule look up telemetry.
 *
 * @param {String} probe The telemetry probe that we want to verify
 * @param {Array} expected An array of objects that describe the expected value.
 */
function verifyLookUpTelemetry(probe, expected) {
  for (let telemetry of expected) {
    Assert.equal(
      telemetry.count,
      Glean.cookieBanners[probe][telemetry.label].testGetValue()
    );
  }
}

/**
 * A helper function to open the testing page for look up telemetry.
 *
 * @param {browser} browser The browser element
 * @param {boolean} testInTop To indicate the page should be opened in top level
 * @param {String} page The url of the testing page
 * @param {String} domain The domain of the testing page
 */
async function openLookUpTelemetryTestPage(browser, testInTop, page, domain) {
  let clickFinishPromise = promiseBannerClickingFinish(domain);

  if (testInTop) {
    BrowserTestUtils.loadURI(browser, page);
  } else {
    BrowserTestUtils.loadURI(browser, TEST_ORIGIN_C);
    await BrowserTestUtils.browserLoaded(browser);

    await SpecialPowers.spawn(browser, [page], async testURL => {
      let iframe = content.document.createElement("iframe");
      iframe.src = testURL;
      content.document.body.appendChild(iframe);
      await ContentTaskUtils.waitForEvent(iframe, "load");
    });
  }

  await clickFinishPromise;
}

add_setup(function() {
  // Clear telemetry before starting telemetry test.
  Services.fog.testResetFOG();

  registerCleanupFunction(async () => {
    Services.prefs.clearUserPref("cookiebanners.service.mode");
    Services.prefs.clearUserPref("cookiebanners.service.mode.privateBrowsing");
    if (
      Services.prefs.getIntPref("cookiebanners.service.mode") !=
        Ci.nsICookieBannerService.MODE_DISABLED ||
      Services.prefs.getIntPref("cookiebanners.service.mode.privateBrowsing") !=
        Ci.nsICookieBannerService.MODE_DISABLED
    ) {
      // Restore original rules.
      Services.cookieBanners.resetRules(true);
    }

    // Clear cookies that have been set during testing.
    await SiteDataTestUtils.clear();
  });
});

add_task(async function test_service_mode_telemetry() {
  let service = Cc["@mozilla.org/cookie-banner-service;1"].getService(
    Ci.nsIObserver
  );

  for (let mode of TEST_MODES) {
    for (let modePBM of TEST_MODES) {
      await SpecialPowers.pushPrefEnv({
        set: [
          ["cookiebanners.service.mode", mode],
          ["cookiebanners.service.mode.privateBrowsing", modePBM],
        ],
      });

      // Trigger the idle-daily on the cookie banner service.
      service.observe(null, "idle-daily", null);

      // Verify the telemetry value.
      for (let label of [
        "disabled",
        "reject",
        "reject_or_accept",
        "detect_only",
        "invalid",
      ]) {
        let expected = convertModeToTelemetryString(mode) == label;
        let expectedPBM = convertModeToTelemetryString(modePBM) == label;

        is(
          Glean.cookieBanners.normalWindowServiceMode[label].testGetValue(),
          expected,
          `Has set label ${label} to ${expected} for mode ${mode}.`
        );
        is(
          Glean.cookieBanners.privateWindowServiceMode[label].testGetValue(),
          expectedPBM,
          `Has set label '${label}' to ${expected} for mode ${modePBM}.`
        );
      }
    }
  }
});

add_task(async function test_rule_lookup_telemetry_no_rule() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
    ],
  });
  await clickTestSetup();

  // Clear out all rules.
  Services.cookieBanners.resetRules(false);

  // Open a tab for testing.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  for (let context of ["top", "iframe"]) {
    let isTop = context === "top";

    // Open a test domain. We should record a rule miss because there is no rule
    // right now
    info("Open a test domain.");
    await openLookUpTelemetryTestPage(
      tab.linkedBrowser,
      isTop,
      TEST_ORIGIN_A,
      TEST_DOMAIN_A
    );

    let expectedTelemetryOnce = [
      {
        label: `${context}_miss`,
        count: 1,
      },
      {
        label: `${context}_cookie_miss`,
        count: 1,
      },
      {
        label: `${context}_click_miss`,
        count: 1,
      },
    ];
    verifyLookUpTelemetry("ruleLookupByLoad", expectedTelemetryOnce);
    verifyLookUpTelemetry("ruleLookupByDomain", expectedTelemetryOnce);

    info("Open the same domain again.");
    // Load the same domain again, verify that the telemetry counts increases for
    // load telemetry not not for domain telemetry.
    await openLookUpTelemetryTestPage(
      tab.linkedBrowser,
      isTop,
      TEST_ORIGIN_A,
      TEST_DOMAIN_A
    );

    let expectedTelemetryTwice = [
      {
        label: `${context}_miss`,
        count: 2,
      },
      {
        label: `${context}_cookie_miss`,
        count: 2,
      },
      {
        label: `${context}_click_miss`,
        count: 2,
      },
    ];
    verifyLookUpTelemetry("ruleLookupByLoad", expectedTelemetryTwice);
    verifyLookUpTelemetry("ruleLookupByDomain", expectedTelemetryOnce);
  }

  Services.fog.testResetFOG();
  Services.cookieBanners.resetDomainTelemetryRecord();
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_rule_lookup_telemetry() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
    ],
  });
  await clickTestSetup();
  insertTestClickRules();

  // Open a tab for testing.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  for (let type of ["click", "cookie"]) {
    info(`Running the test for lookup telemetry for ${type} rules.`);
    // Clear out all rules.
    Services.cookieBanners.resetRules(false);

    info("Insert rules.");
    if (type === "click") {
      insertTestClickRules();
    } else {
      let ruleA = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
        Ci.nsICookieBannerRule
      );
      ruleA.domains = [TEST_DOMAIN_A];

      Services.cookieBanners.insertRule(ruleA);
      ruleA.addCookie(
        true,
        `cookieConsent_${TEST_DOMAIN_A}_1`,
        "optOut1",
        "." + TEST_DOMAIN_A,
        "/",
        3600,
        "",
        false,
        false,
        false,
        0,
        0
      );
      ruleA.addCookie(
        false,
        `cookieConsent_${TEST_DOMAIN_A}_2`,
        "optIn2",
        TEST_DOMAIN_A,
        "/",
        3600,
        "",
        false,
        false,
        false,
        0,
        0
      );

      let ruleB = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
        Ci.nsICookieBannerRule
      );
      ruleB.domains = [TEST_DOMAIN_B];

      Services.cookieBanners.insertRule(ruleB);
      ruleB.addCookie(
        false,
        `cookieConsent_${TEST_DOMAIN_B}_1`,
        "optIn1",
        TEST_DOMAIN_B,
        "/",
        3600,
        "UNSET",
        false,
        false,
        true,
        0,
        0
      );
    }

    for (let context of ["top", "iframe"]) {
      info(`Test in a ${context} context.`);
      let isTop = context === "top";

      info("Load a domain with opt-in and opt-out clicking rules.");
      await openLookUpTelemetryTestPage(
        tab.linkedBrowser,
        isTop,
        TEST_ORIGIN_A,
        TEST_DOMAIN_A
      );

      let expectedTelemetry = [
        {
          label: `${context}_hit`,
          count: 1,
        },
        {
          label: `${context}_hit_opt_in`,
          count: 1,
        },
        {
          label: `${context}_hit_opt_out`,
          count: 1,
        },
        {
          label: `${context}_${type}_hit`,
          count: 1,
        },
        {
          label: `${context}_${type}_hit_opt_in`,
          count: 1,
        },
        {
          label: `${context}_${type}_hit_opt_out`,
          count: 1,
        },
      ];
      verifyLookUpTelemetry("ruleLookupByLoad", expectedTelemetry);
      verifyLookUpTelemetry("ruleLookupByDomain", expectedTelemetry);

      info("Load a domain with only opt-in clicking rules");
      await openLookUpTelemetryTestPage(
        tab.linkedBrowser,
        isTop,
        TEST_ORIGIN_B,
        TEST_DOMAIN_B
      );

      expectedTelemetry = [
        {
          label: `${context}_hit`,
          count: 2,
        },
        {
          label: `${context}_hit_opt_in`,
          count: 2,
        },
        {
          label: `${context}_hit_opt_out`,
          count: 1,
        },
        {
          label: `${context}_${type}_hit`,
          count: 2,
        },
        {
          label: `${context}_${type}_hit_opt_in`,
          count: 2,
        },
        {
          label: `${context}_${type}_hit_opt_out`,
          count: 1,
        },
      ];

      verifyLookUpTelemetry("ruleLookupByLoad", expectedTelemetry);
      verifyLookUpTelemetry("ruleLookupByDomain", expectedTelemetry);

      info(
        "Load a domain again to verify that we don't collect domain telemetry for this time."
      );
      await openLookUpTelemetryTestPage(
        tab.linkedBrowser,
        isTop,
        TEST_ORIGIN_A,
        TEST_DOMAIN_A
      );

      // The domain telemetry should't be changed.
      verifyLookUpTelemetry("ruleLookupByDomain", expectedTelemetry);

      expectedTelemetry = [
        {
          label: `${context}_hit`,
          count: 3,
        },
        {
          label: `${context}_hit_opt_in`,
          count: 3,
        },
        {
          label: `${context}_hit_opt_out`,
          count: 2,
        },
        {
          label: `${context}_${type}_hit`,
          count: 3,
        },
        {
          label: `${context}_${type}_hit_opt_in`,
          count: 3,
        },
        {
          label: `${context}_${type}_hit_opt_out`,
          count: 2,
        },
      ];

      // Verify that the load telemetry still increases.
      verifyLookUpTelemetry("ruleLookupByLoad", expectedTelemetry);
    }

    Services.fog.testResetFOG();
    Services.cookieBanners.resetDomainTelemetryRecord();
  }

  BrowserTestUtils.removeTab(tab);
});
