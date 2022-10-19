/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// TODO: Split up the test into multiple tests (Bug 1795238)
requestLongerTimeout(3);

add_setup(clickTestSetup);

/**
 * Test that the banner clicking won't click banner if the service is disabled.
 */
add_task(async function test_cookie_banner_service_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_DISABLED],
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_DISABLED,
      ],
    ],
  });

  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: true,
    expected: "NoClick",
  });
});

/**
 * Test that the banner clicking won't click banner if there is no rule.
 */
add_task(async function test_no_rules() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  info("Clearing existing rules");
  Services.cookieBanners.resetRules(false);

  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: true,
    expected: "NoClick",
  });
});

/**
 * Test the banner clicking with MODE_REJECT.
 */
add_task(async function test_clicking_mode_reject() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  insertTestClickRules();

  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  // No opt out rule for the example.org, the banner shouldn't be clicked.
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_B,
    testURL: TEST_PAGE_B,
    visible: true,
    expected: "NoClick",
  });
});

/**
 * Test the banner clicking with MODE_REJECT_OR_ACCEPT.
 */
add_task(async function test_clicking_mode_reject_or_accept() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
    ],
  });

  insertTestClickRules();

  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_B,
    testURL: TEST_PAGE_B,
    visible: false,
    expected: "OptIn",
  });
});

/**
 * Test the banner clicking with the case where the banner is added after
 * page loads and with a short amount of delay.
 */
add_task(async function test_clicking_with_delayed_banner() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  insertTestClickRules();

  let TEST_PAGE =
    TEST_ORIGIN_A + TEST_PATH + "file_delayed_banner.html?delay=100";
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE,
    visible: false,
    expected: "OptOut",
  });
});

/**
 * Test that the banner clicking in an iframe.
 */
add_task(async function test_embedded_iframe() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  insertTestClickRules();

  await openIframeAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });
});

/**
 * Test banner clicking with the private browsing window.
 */
add_task(async function test_pbm() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_REJECT,
      ],
    ],
  });

  insertTestClickRules();

  let pbmWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  await openPageAndVerify({
    win: pbmWindow,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  await BrowserTestUtils.closeWindow(pbmWindow);
});

/**
 * Tests service mode pref combinations for normal and private browsing.
 */
add_task(async function test_pref_pbm_pref() {
  info("Enable in normal browsing but disable in private browsing.");
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_DISABLED,
      ],
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
    ],
  });

  insertTestClickRules();

  let pbmWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  await openPageAndVerify({
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  await openPageAndVerify({
    win: pbmWindow,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: true,
    expected: "NoClick",
  });

  info("Disable in normal browsing but enable in private browsing.");
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_REJECT,
      ],
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_DISABLED],
    ],
  });

  await openPageAndVerify({
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: true,
    expected: "NoClick",
  });

  await openPageAndVerify({
    win: pbmWindow,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  info(
    "Set normal browsing to REJECT_OR_ACCEPT and private browsing to REJECT."
  );
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_REJECT,
      ],
      [
        "cookiebanners.service.mode",
        Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      ],
    ],
  });

  info(
    "The normal browsing window accepts the banner according to the opt-in rule."
  );
  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_B,
    testURL: TEST_PAGE_B,
    visible: false,
    expected: "OptIn",
  });

  info(
    "The private browsing window should not perform any click, because there is only an opt-in rule."
  );
  await openPageAndVerify({
    win: pbmWindow,
    domain: TEST_DOMAIN_B,
    testURL: TEST_PAGE_B,
    visible: true,
    expected: "NoClick",
  });

  await BrowserTestUtils.closeWindow(pbmWindow);
});

/**
 * Test that the banner clicking in an iframe with the private browsing window.
 */
add_task(async function test_embedded_iframe_pbm() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_REJECT,
      ],
    ],
  });

  insertTestClickRules();

  let pbmWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  await openIframeAndVerify({
    win: pbmWindow,
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });

  await BrowserTestUtils.closeWindow(pbmWindow);
});

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
  ruleA.domain = "*";
  ruleA.addClickRule("div#banner", null, "button#optOut", "button#optIn");
  Services.cookieBanners.insertRule(ruleA);

  info(
    "Add global ruleC which targets an existing banner (presence) but non-existing buttons."
  );
  let ruleC = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleC.id = genUUID();
  ruleC.domain = "*";
  ruleC.addClickRule(
    "div#banner",
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
  ruleD.domain = "*";
  ruleD.addClickRule("div#nonExistingBanner", null, null, "button#optIn");
  Services.cookieBanners.insertRule(ruleD);

  info("The global rule ruleA should handle both test pages with div#banner.");
  await openPageAndVerify({
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    expected: "OptOut",
  });
  await openPageAndVerify({
    domain: TEST_DOMAIN_B,
    testURL: TEST_PAGE_B,
    visible: false,
    expected: "OptOut",
  });

  info("No global rule should handle TEST_PAGE_C with div#bannerB.");
  await openPageAndVerify({
    domain: TEST_DOMAIN_C,
    testURL: TEST_PAGE_C,
    visible: true,
    expected: "NoClick",
    bannerId: "bannerB",
  });

  info("Test delayed banner handling with global rules.");
  let TEST_PAGE =
    TEST_ORIGIN_A + TEST_PATH + "file_delayed_banner.html?delay=100";
  await openPageAndVerify({
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE,
    visible: false,
    expected: "OptOut",
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
  ruleGlobal.domain = "*";
  ruleGlobal.addClickRule("div#banner", null, "button#optOut", null);
  Services.cookieBanners.insertRule(ruleGlobal);

  info("Add domain specific rule which also targets the existing banner.");
  let ruleDomain = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleDomain.id = genUUID();
  ruleDomain.domain = TEST_DOMAIN_A;
  ruleDomain.addClickRule("div#banner", null, null, "button#optIn");
  Services.cookieBanners.insertRule(ruleDomain);

  info("Test that the domain-specific rule applies, not the global one.");
  await openPageAndVerify({
    domain: TEST_DOMAIN_A,
    testURL: TEST_PAGE_A,
    visible: false,
    // Because of the way the rules are setup OptOut would mean the global rule
    // applies, opt-in means the domain specific rule applies.
    expected: "OptIn",
  });
});

/**
 * Test that domain preference takes precedence over pref settings.
 */
add_task(async function test_domain_preference() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_REJECT,
      ],
    ],
  });

  insertTestClickRules();

  for (let testPBM of [false, true]) {
    let testWin = window;
    if (testPBM) {
      testWin = await BrowserTestUtils.openNewBrowserWindow({
        private: true,
      });
    }

    info(
      "Make sure the example.org follows the pref setting when there is no domain preference."
    );
    await openPageAndVerify({
      win: testWin,
      domain: TEST_DOMAIN_B,
      testURL: TEST_PAGE_B,
      visible: true,
      expected: "NoClick",
    });

    info("Set the domain preference of example.org to MODE_REJECT_OR_ACCEPT");
    let uri = Services.io.newURI(TEST_ORIGIN_B);
    Services.cookieBanners.setDomainPref(
      uri,
      Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      testPBM
    );

    info(
      "Verify if domain preference takes precedence over then the pref setting for example.org"
    );
    await openPageAndVerify({
      win: testWin,
      domain: TEST_DOMAIN_B,
      testURL: TEST_PAGE_B,
      visible: false,
      expected: "OptIn",
    });

    Services.cookieBanners.removeAllDomainPrefs(testPBM);

    if (testPBM) {
      await BrowserTestUtils.closeWindow(testWin);
    }
  }
});

/**
 * Test that domain preference works on the top-level domain.
 */
add_task(async function test_domain_preference_iframe() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", Ci.nsICookieBannerService.MODE_REJECT],
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_REJECT,
      ],
    ],
  });

  insertTestClickRules();

  for (let testPBM of [false, true]) {
    let testWin = window;
    if (testPBM) {
      testWin = await BrowserTestUtils.openNewBrowserWindow({
        private: true,
      });
    }

    info(
      "Make sure the example.org follows the pref setting when there is no domain preference for the top-level example.net."
    );
    await openIframeAndVerify({
      win: testWin,
      domain: TEST_DOMAIN_B,
      testURL: TEST_PAGE_B,
      visible: true,
      expected: "NoClick",
    });

    info(
      "Set the domain preference of the top-level domain to MODE_REJECT_OR_ACCEPT"
    );
    let uri = Services.io.newURI(TEST_ORIGIN_C);
    Services.cookieBanners.setDomainPref(
      uri,
      Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT,
      testPBM
    );

    info(
      "Verify if domain preference takes precedence over then the pref setting for top-level example.net"
    );
    await openIframeAndVerify({
      win: testWin,
      domain: TEST_DOMAIN_B,
      testURL: TEST_PAGE_B,
      visible: false,
      expected: "OptIn",
    });

    Services.cookieBanners.removeAllDomainPrefs(testPBM);

    if (testPBM) {
      await BrowserTestUtils.closeWindow(testWin);
    }
  }
});
