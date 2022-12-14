/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_DOMAIN_A = "example.com";
const TEST_DOMAIN_B = "example.org";
const TEST_DOMAIN_C = "example.net";

const TEST_ORIGIN_A = "https://" + TEST_DOMAIN_A;
const TEST_ORIGIN_B = "https://" + TEST_DOMAIN_B;
const TEST_ORIGIN_C = "https://" + TEST_DOMAIN_C;

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  ""
);

const TEST_PAGE_A = TEST_ORIGIN_A + TEST_PATH + "file_banner.html";
const TEST_PAGE_B = TEST_ORIGIN_B + TEST_PATH + "file_banner.html";
// Page C has a different banner element ID than A and B.
const TEST_PAGE_C = TEST_ORIGIN_C + TEST_PATH + "file_banner_b.html";

function genUUID() {
  return Services.uuid.generateUUID().number.slice(1, -1);
}

/**
 * Common setup function for banner click tests.
 */
async function clickTestSetup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Enable debug logging.
      ["cookiebanners.listService.logLevel", "Debug"],
      ["cookiebanners.bannerClicking.logLevel", "Debug"],
      ["cookiebanners.bannerClicking.testing", true],
      ["cookiebanners.bannerClicking.timeout", 500],
      ["cookiebanners.bannerClicking.enabled", true],
    ],
  });

  // Reset GLEAN (FOG) telemetry to avoid data bleeding over from other tests.
  Services.fog.testResetFOG();

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("cookiebanners.service.mode");
    Services.prefs.clearUserPref("cookiebanners.service.mode.privateBrowsing");
    if (Services.cookieBanners.isEnabled) {
      // Restore original rules.
      Services.cookieBanners.resetRules(true);
    }
  });
}

/**
 * A helper function returns a promise which resolves when the banner clicking
 * is finished for the given domain.
 *
 * @param {String} domain the domain that should run the banner clicking.
 */
function promiseBannerClickingFinish(domain) {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(subject, topic, data) {
      if (data != domain) {
        return;
      }

      Services.obs.removeObserver(
        observer,
        "cookie-banner-test-clicking-finish"
      );
      resolve();
    }, "cookie-banner-test-clicking-finish");
  });
}

/**
 * A helper function to verify the banner state of the given browsingContext.
 *
 * @param {BrowsingContext} bc - the browsing context
 * @param {boolean} visible - if the banner should be visible.
 * @param {boolean} expected - the expected banner click state.
 * @param {string} [bannerId] - id of the cookie banner element.
 */
async function verifyBannerState(bc, visible, expected, bannerId = "banner") {
  info("Verify the cookie banner state.");

  await SpecialPowers.spawn(
    bc,
    [visible, expected, bannerId],
    (visible, expected, bannerId) => {
      let banner = content.document.getElementById(bannerId);

      is(
        banner.checkVisibility({
          checkOpacity: true,
          checkVisibilityCSS: true,
        }),
        visible,
        `The banner element should be ${visible ? "visible" : "hidden"}`
      );

      let result = content.document.getElementById("result");

      is(result.textContent, expected, "The build click state is correct.");
    }
  );
}

/**
 * A helper function to open the test page and verify the banner state.
 *
 * @param {Window} [win] - the chrome window object.
 * @param {String} domain - the domain of the testing page.
 * @param {String} testURL - the url of the testing page.
 * @param {boolean} visible - if the banner should be visible.
 * @param {boolean} expected - the expected banner click state.
 * @param {string} [bannerId] - id of the cookie banner element.
 * @param {boolean} [keepTabOpen] - whether to leave the tab open after the test
 * function completed.
 */
async function openPageAndVerify({
  win = window,
  domain,
  testURL,
  visible,
  expected,
  bannerId = "banner",
  keepTabOpen = false,
}) {
  info(`Opening ${testURL}`);

  let promise = promiseBannerClickingFinish(domain);

  let tab = await BrowserTestUtils.openNewForegroundTab(win.gBrowser, testURL);

  await promise;

  await verifyBannerState(tab.linkedBrowser, visible, expected, bannerId);

  if (!keepTabOpen) {
    BrowserTestUtils.removeTab(tab);
  }
}

/**
 * A helper function to open the test page in an iframe and verify the banner
 * state in the iframe.
 *
 * @param {Window} win - the chrome window object.
 * @param {String} domain - the domain of the testing iframe page.
 * @param {String} testURL - the url of the testing iframe page.
 * @param {boolean} visible - if the banner should be visible.
 * @param {boolean} expected - the expected banner click state.
 */
async function openIframeAndVerify({
  win,
  domain,
  testURL,
  visible,
  expected,
}) {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    win.gBrowser,
    TEST_ORIGIN_C
  );

  let promise = promiseBannerClickingFinish(domain);

  let iframeBC = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [testURL],
    async testURL => {
      let iframe = content.document.createElement("iframe");
      iframe.src = testURL;
      content.document.body.appendChild(iframe);
      await ContentTaskUtils.waitForEvent(iframe, "load");

      return iframe.browsingContext;
    }
  );

  await promise;
  await verifyBannerState(iframeBC, visible, expected);

  BrowserTestUtils.removeTab(tab);
}

/**
 * A helper function to insert testing rules.
 */
function insertTestClickRules() {
  info("Clearing existing rules");
  Services.cookieBanners.resetRules(false);

  info("Inserting test rules.");

  info("Add opt-out click rule for DOMAIN_A.");
  let ruleA = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleA.id = genUUID();
  ruleA.domains = [TEST_DOMAIN_A];

  ruleA.addClickRule(
    "div#banner",
    false,
    Ci.nsIClickRule.RUN_ALL,
    null,
    "button#optOut",
    "button#optIn"
  );
  Services.cookieBanners.insertRule(ruleA);

  info("Add opt-in click rule for DOMAIN_B.");
  let ruleB = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleB.id = genUUID();
  ruleB.domains = [TEST_DOMAIN_B];

  ruleB.addClickRule(
    "div#banner",
    false,
    Ci.nsIClickRule.RUN_ALL,
    null,
    null,
    "button#optIn"
  );
  Services.cookieBanners.insertRule(ruleB);

  info("Add global ruleC which targets a non-existing banner (presence).");
  let ruleC = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleC.id = genUUID();
  ruleC.domains = [];
  ruleC.addClickRule(
    "div#nonExistingBanner",
    false,
    Ci.nsIClickRule.RUN_ALL,
    null,
    null,
    "button#optIn"
  );
  Services.cookieBanners.insertRule(ruleC);

  info("Add global ruleD which targets a non-existing banner (presence).");
  let ruleD = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleD.id = genUUID();
  ruleD.domains = [];
  ruleD.addClickRule(
    "div#nonExistingBanner2",
    false,
    Ci.nsIClickRule.RUN_ALL,
    null,
    "button#optOut",
    "button#optIn"
  );
  Services.cookieBanners.insertRule(ruleD);
}

/**
 * Test the Glean.cookieBannersClick.result metric.
 * @param {*} expected - Object mapping labels to counters. Omitted labels are
 * asserted to be in initial state (undefined =^ 0)
 * @param {boolean} [resetFOG] - Whether to reset all FOG telemetry after the
 * method has finished.
 */
async function testClickResultTelemetry(expected, resetFOG = true) {
  // Ensure we have all data from the content process.
  await Services.fog.testFlushAllChildren();

  let labels = [
    "success",
    "success_dom_content_loaded",
    "success_mutation_pre_load",
    "success_mutation_post_load",
    "fail",
    "fail_banner_not_found",
    "fail_banner_not_visible",
    "fail_button_not_found",
    "fail_no_rule_for_mode",
    "fail_actor_destroyed",
  ];

  let testMetricState = doAssert => {
    for (let label of labels) {
      if (doAssert) {
        is(
          Glean.cookieBannersClick.result[label].testGetValue(),
          expected[label],
          `Counter for label '${label}' has correct state.`
        );
      } else if (
        Glean.cookieBannersClick.result[label].testGetValue() !==
        expected[label]
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

    // Reset telemetry, even if the test condition above throws. This is to
    // avoid failing subsequent tests in case of a test failure.
    if (resetFOG) {
      Services.fog.testResetFOG();
    }
  }
}
