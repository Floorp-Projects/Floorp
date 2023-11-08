/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SiteDataTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SiteDataTestUtils.sys.mjs"
);

const { MODE_DISABLED, MODE_REJECT, MODE_REJECT_OR_ACCEPT, MODE_UNSET } =
  Ci.nsICookieBannerService;

const TEST_MODES = [
  MODE_DISABLED,
  MODE_REJECT,
  MODE_REJECT_OR_ACCEPT,
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
 * A helper function to verify the reload telemetry.
 *
 * @param {Number} length The expected length of the telemetry array.
 * @param {Number} idx The index of the telemetry to be verified.
 * @param {Object} expected An object that describe the expected value.
 */
function verifyReloadTelemetry(length, idx, expected) {
  let events = Glean.cookieBanners.reload.testGetValue();

  is(events.length, length, "There is a expected number of reload events.");

  let event = events[idx];

  let { noRule, hasCookieRule, hasClickRule } = expected;
  is(event.name, "reload", "The reload event has the correct name");
  is(event.extra.no_rule, noRule, "The extra field 'no_rule' is expected");
  is(
    event.extra.has_cookie_rule,
    hasCookieRule,
    "The extra field 'has_cookie_rule' is expected"
  );
  is(
    event.extra.has_click_rule,
    hasClickRule,
    "The extra field 'has_click_rule' is expected"
  );
}

/**
 * A helper function to reload the browser and wait until it loads.
 *
 * @param {Browser} browser The browser object.
 * @param {String} url The URL to be loaded.
 */
async function reloadBrowser(browser, url) {
  let reloaded = BrowserTestUtils.browserLoaded(browser, false, url);

  // Reload as a user.
  window.BrowserReload();

  await reloaded;
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
    BrowserTestUtils.startLoadingURIString(browser, page);
  } else {
    BrowserTestUtils.startLoadingURIString(browser, TEST_ORIGIN_C);
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

add_setup(async function () {
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

  await clickTestSetup();
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
      for (let label of ["disabled", "reject", "reject_or_accept", "invalid"]) {
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

      await SpecialPowers.popPrefEnv();
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

  // Clear out all rules.
  Services.cookieBanners.resetRules(false);

  // Open a tab for testing.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  for (let context of ["top", "iframe"]) {
    let isTop = context === "top";

    // Clear the executed records before testing.
    Services.cookieBanners.removeAllExecutedRecords(false);

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

    Services.cookieBanners.removeAllExecutedRecords(false);

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
  insertTestClickRules();

  // Clear the executed records before testing.
  Services.cookieBanners.removeAllExecutedRecords(false);

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
        null,
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
        null,
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
        null,
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
      // Clear the executed records before testing.
      Services.cookieBanners.removeAllExecutedRecords(false);

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

      Services.cookieBanners.removeAllExecutedRecords(false);

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

      Services.cookieBanners.removeAllExecutedRecords(false);

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

add_task(async function test_reload_telemetry_no_rule() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
    ],
  });

  // Clear out all rules.
  Services.cookieBanners.resetRules(false);

  // Open a tab for testing.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_ORIGIN_A
  );

  // Make sure there is no reload event at the beginning.
  let events = Glean.cookieBanners.reload.testGetValue();
  ok(!events, "No reload event at the beginning.");

  // Trigger the reload
  await reloadBrowser(tab.linkedBrowser, TEST_ORIGIN_A + "/");

  // Check the telemetry
  verifyReloadTelemetry(1, 0, {
    noRule: "true",
    hasCookieRule: "false",
    hasClickRule: "false",
  });

  BrowserTestUtils.removeTab(tab);
  Services.fog.testResetFOG();
});

add_task(async function test_reload_telemetry() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
    ],
  });
  insertTestClickRules();

  // Open a tab for testing.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_ORIGIN_A
  );

  // Make sure there is no reload event at the beginning.
  let events = Glean.cookieBanners.reload.testGetValue();
  ok(!events, "No reload event at the beginning.");

  // Trigger the reload
  await reloadBrowser(tab.linkedBrowser, TEST_ORIGIN_A + "/");

  // Check the telemetry
  verifyReloadTelemetry(1, 0, {
    noRule: "false",
    hasCookieRule: "false",
    hasClickRule: "true",
  });

  // Add a both click rule and cookie rule for another domain.
  let cookieRule = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  cookieRule.domains = [TEST_DOMAIN_B];

  Services.cookieBanners.insertRule(cookieRule);
  cookieRule.addCookie(
    false,
    `cookieConsent_${TEST_DOMAIN_B}_1`,
    "optIn1",
    null,
    "/",
    3600,
    "UNSET",
    false,
    false,
    true,
    0,
    0
  );
  cookieRule.addClickRule(
    "div#banner",
    false,
    Ci.nsIClickRule.RUN_ALL,
    null,
    null,
    "button#optIn"
  );

  // Load the page with another origin.
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, TEST_ORIGIN_B);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  // Trigger the reload
  await reloadBrowser(tab.linkedBrowser, TEST_ORIGIN_B + "/");

  // Check the telemetry
  verifyReloadTelemetry(2, 1, {
    noRule: "false",
    hasCookieRule: "true",
    hasClickRule: "true",
  });

  BrowserTestUtils.removeTab(tab);
  Services.fog.testResetFOG();
});

add_task(async function test_reload_telemetry_mode_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
    ],
  });
  insertTestClickRules();

  // Disable the cookie banner service in normal browsing.
  // Keep it enabled in PBM so the service stays alive and can still collect telemetry.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_DISABLED],
    ],
  });

  // Open a tab for testing.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_ORIGIN_A
  );

  // Trigger the reload
  await reloadBrowser(tab.linkedBrowser, TEST_ORIGIN_A + "/");

  // Check the telemetry. The reload telemetry should report no rule given that
  // the service is disabled.
  verifyReloadTelemetry(1, 0, {
    noRule: "true",
    hasCookieRule: "false",
    hasClickRule: "false",
  });

  BrowserTestUtils.removeTab(tab);
  Services.fog.testResetFOG();
});

add_task(async function test_reload_telemetry_mode_reject() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });
  insertTestClickRules();

  // Open a tab for testing.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_ORIGIN_A
  );

  // Trigger the reload
  await reloadBrowser(tab.linkedBrowser, TEST_ORIGIN_A + "/");

  // Check the telemetry. The reload telemetry should report there is click rule
  // for the domain has opt-out rule.
  verifyReloadTelemetry(1, 0, {
    noRule: "false",
    hasCookieRule: "false",
    hasClickRule: "true",
  });

  // Load the page with the domain only has opt-in click rule.
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, TEST_ORIGIN_B);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  // Trigger the reload
  await reloadBrowser(tab.linkedBrowser, TEST_ORIGIN_B + "/");

  // Check the telemetry. It should report there is no rule because the domain
  // only has an opt-in click rule.
  verifyReloadTelemetry(2, 1, {
    noRule: "true",
    hasCookieRule: "false",
    hasClickRule: "false",
  });

  BrowserTestUtils.removeTab(tab);
  Services.fog.testResetFOG();
});

add_task(async function test_reload_telemetry_iframe() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
    ],
  });

  // Clear out all rules.
  Services.cookieBanners.resetRules(false);

  // Insert a click rule for an iframe case. And add a cookie rule for the same
  // domain. We shouldn't report there is a cookie rule for iframe because
  // cookie rules are top-level only.
  let cookieRule = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  cookieRule.domains = [TEST_DOMAIN_A];
  Services.cookieBanners.insertRule(cookieRule);

  cookieRule.addClickRule(
    "div#banner",
    false,
    Ci.nsIClickRule.RUN_CHILD,
    null,
    null,
    "button#optIn"
  );
  cookieRule.addCookie(
    false,
    `cookieConsent_${TEST_DOMAIN_A}_1`,
    "optIn1",
    null,
    "/",
    3600,
    "UNSET",
    false,
    false,
    true,
    0,
    0
  );

  // Open a tab for testing.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_ORIGIN_C + TEST_PATH + "file_iframe_banner.html"
  );

  // Trigger the reload
  await reloadBrowser(
    tab.linkedBrowser,
    TEST_ORIGIN_C + TEST_PATH + "file_iframe_banner.html"
  );

  // Check the telemetry
  verifyReloadTelemetry(1, 0, {
    noRule: "false",
    hasCookieRule: "false",
    hasClickRule: "true",
  });

  BrowserTestUtils.removeTab(tab);
  Services.fog.testResetFOG();
});

add_task(async function test_service_detectOnly_telemetry() {
  let service = Cc["@mozilla.org/cookie-banner-service;1"].getService(
    Ci.nsIObserver
  );

  for (let detectOnly of [true, false, true]) {
    await SpecialPowers.pushPrefEnv({
      set: [["cookiebanners.service.detectOnly", detectOnly]],
    });

    // Trigger the idle-daily on the cookie banner service.
    service.observe(null, "idle-daily", null);

    is(
      Glean.cookieBanners.serviceDetectOnly.testGetValue(),
      detectOnly,
      `Has set detect-only metric to ${detectOnly}.`
    );

    await SpecialPowers.popPrefEnv();
  }
});
