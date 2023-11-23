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
 * Common setup function for cookie banner handling tests.
 */
async function testSetup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Enable debug logging.
      ["cookiebanners.listService.logLevel", "Debug"],
      // Avoid importing rules from RemoteSettings. They may interfere with test
      // rules / assertions.
      ["cookiebanners.listService.testSkipRemoteSettings", true],
    ],
  });

  // Reset GLEAN (FOG) telemetry to avoid data bleeding over from other tests.
  await Services.fog.testFlushAllChildren();
  Services.fog.testResetFOG();

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("cookiebanners.service.mode");
    Services.prefs.clearUserPref("cookiebanners.service.mode.privateBrowsing");
    if (Services.cookieBanners.isEnabled) {
      // Restore original rules.
      Services.cookieBanners.resetRules(true);

      // Clear executed records.
      Services.cookieBanners.removeAllExecutedRecords(false);
      Services.cookieBanners.removeAllExecutedRecords(true);
    }
  });
}

/**
 * Setup function for click tests.
 */
async function clickTestSetup() {
  await testSetup();

  await SpecialPowers.pushPrefEnv({
    set: [
      // Enable debug logging.
      ["cookiebanners.bannerClicking.logLevel", "Debug"],
      ["cookiebanners.bannerClicking.testing", true],
      ["cookiebanners.bannerClicking.timeoutAfterLoad", 500],
      ["cookiebanners.bannerClicking.enabled", true],
      ["cookiebanners.cookieInjector.enabled", false],
    ],
  });
}

/**
 * Setup function for cookie injector tests.
 */
async function cookieInjectorTestSetup() {
  await testSetup();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.cookieInjector.enabled", true],
      // Required to dispatch cookiebanner events.
      ["cookiebanners.bannerClicking.enabled", true],
    ],
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
  expectActorEnabled = true,
}) {
  info(`Opening ${testURL}`);

  // If the actor isn't enabled there won't be a "finished" observer message.
  let promise = expectActorEnabled
    ? promiseBannerClickingFinish(domain)
    : new Promise(resolve => setTimeout(resolve, 0));

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
function insertTestClickRules(insertGlobalRules = true) {
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

  if (insertGlobalRules) {
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
}

/**
 * Inserts cookie injection test rules for TEST_DOMAIN_A and TEST_DOMAIN_B.
 */
function insertTestCookieRules() {
  info("Clearing existing rules");
  Services.cookieBanners.resetRules(false);

  info("Inserting test rules.");

  let ruleA = Cc["@mozilla.org/cookie-banner-rule;1"].createInstance(
    Ci.nsICookieBannerRule
  );
  ruleA.domains = [TEST_DOMAIN_A, TEST_DOMAIN_C];

  Services.cookieBanners.insertRule(ruleA);
  ruleA.addCookie(
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
  ruleA.addCookie(
    true,
    `cookieConsent_${TEST_DOMAIN_A}_2`,
    "optOut2",
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

  // An opt-in cookie rule for DOMAIN_B.
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

/**
 * Test the Glean.cookieBannersClick.result metric.
 *
 * @param {*} expected - Object mapping labels to counters. Omitted labels are
 * asserted to be in initial state (undefined =^ 0)
 * @param {boolean} [resetFOG] - Whether to reset all FOG telemetry after the
 * method has finished.
 */
function testClickResultTelemetry(expected, resetFOG = true) {
  return testClickResultTelemetryInternal(
    Glean.cookieBannersClick.result,
    expected,
    resetFOG
  );
}

/**
 * Test the Glean.cookieBannersCmp.result metric.
 *
 * @param {*} expected - Object mapping labels to counters. Omitted labels are
 * asserted to be in initial state (undefined =^ 0)
 * @param {boolean} [resetFOG] - Whether to reset all FOG telemetry after the
 * method has finished.
 */
function testCMPResultTelemetry(expected, resetFOG = true) {
  return testClickResultTelemetryInternal(
    Glean.cookieBannersCmp.result,
    expected,
    resetFOG
  );
}

/**
 * Test the result metric.
 *
 * @param {Object} targetTelemetry - The target glean interface to run the
 * checks
 * @param {*} expected - Object mapping labels to counters. Omitted labels are
 * asserted to be in initial state (undefined =^ 0)
 * @param {boolean} [resetFOG] - Whether to reset all FOG telemetry after the
 * method has finished.
 */
async function testClickResultTelemetryInternal(
  targetTelemetry,
  expected,
  resetFOG
) {
  // TODO: Bug 1805653: Enable tests for Linux.
  if (AppConstants.platform == "linux") {
    ok(true, "Skip click telemetry tests on linux.");
    return;
  }

  // Ensure we have all data from the content process.
  await Services.fog.testFlushAllChildren();

  let labels = [
    "success",
    "success_cookie_injected",
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
      let expectedValue = expected[label] ?? null;
      if (doAssert) {
        is(
          targetTelemetry[label].testGetValue(),
          expectedValue,
          `Counter for label '${label}' has correct state.`
        );
      } else if (targetTelemetry[label].testGetValue() !== expectedValue) {
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
      await Services.fog.testFlushAllChildren();
      Services.fog.testResetFOG();
    }
  }
}

/**
 * Triggers a cookie banner handling feature and tests the events dispatched.
 * @param {*} options - Test options.
 * @param {nsICookieBannerService::Modes} options.mode - The cookie banner
 * service mode to test with.
 * @param {boolean} options.detectOnly - Whether the service should be enabled
 * in detection only mode, where it does not handle banners.
 * @param {function} options.initFn - Function to call for test initialization.
 * @param {function} options.triggerFn - Function to call to trigger the banner
 * handling feature.
 * @param {string} options.testURL - URL where the test will trigger the banner
 * handling feature.
 * @returns {Promise} Resolves when the test completes, after cookie banner
 * events.
 */
async function runEventTest({ mode, detectOnly, initFn, triggerFn, testURL }) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["cookiebanners.service.mode", mode],
      [
        "cookiebanners.service.mode.privateBrowsing",
        Ci.nsICookieBannerService.MODE_DISABLED,
      ],
      ["cookiebanners.service.detectOnly", detectOnly],
    ],
  });

  await initFn();

  let expectEventDetected = mode != Ci.nsICookieBannerService.MODE_DISABLED;
  let expectEventHandled =
    !detectOnly &&
    (mode == Ci.nsICookieBannerService.MODE_REJECT ||
      mode == Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT);

  let eventObservedDetected = false;
  let eventObservedHandled = false;

  // This is a bit hacky, we use side effects caused by the checkFn we pass into
  // waitForEvent to keep track of whether an event fired.
  let promiseEventDetected = BrowserTestUtils.waitForEvent(
    window,
    "cookiebannerdetected",
    false,
    () => {
      eventObservedDetected = true;
      return true;
    }
  );
  let promiseEventHandled = BrowserTestUtils.waitForEvent(
    window,
    "cookiebannerhandled",
    false,
    () => {
      eventObservedHandled = true;
      return true;
    }
  );

  // If we expect any events check which one comes first.
  let firstEventPromise;
  if (expectEventDetected || expectEventHandled) {
    firstEventPromise = Promise.race([
      promiseEventHandled,
      promiseEventDetected,
    ]);
  }

  await triggerFn();

  let eventDetected;
  if (expectEventDetected) {
    eventDetected = await promiseEventDetected;

    is(
      eventDetected.type,
      "cookiebannerdetected",
      "Should dispatch cookiebannerdetected event."
    );
  }

  let eventHandled;
  if (expectEventHandled) {
    eventHandled = await promiseEventHandled;
    is(
      eventHandled.type,
      "cookiebannerhandled",
      "Should dispatch cookiebannerhandled event."
    );
  }

  // For MODE_DISABLED this array will be empty, because we don't expect any
  // events to be dispatched.
  let eventsToTest = [eventDetected, eventHandled].filter(event => !!event);

  for (let event of eventsToTest) {
    info(`Testing properties of event ${event.type}`);

    let { windowContext } = event.detail;
    ok(
      windowContext,
      `Event ${event.type} detail should contain a WindowContext`
    );

    let browser = windowContext.browsingContext.top.embedderElement;
    ok(
      browser,
      "WindowContext should have an associated top embedder element."
    );
    is(
      browser.tagName,
      "browser",
      "The top embedder element should be a browser"
    );
    let chromeWin = browser.ownerGlobal;
    is(
      chromeWin,
      window,
      "The chrome window associated with the browser should match the window where the cookie banner was handled."
    );
    is(
      chromeWin.gBrowser.selectedBrowser,
      browser,
      "The browser associated with the event should be the selected browser."
    );
    is(
      browser.currentURI.spec,
      testURL,
      "The browser's URI spec should match the cookie banner test page."
    );
  }

  let firstEvent = await firstEventPromise;
  is(
    expectEventDetected || expectEventHandled,
    !!firstEvent,
    "Should have observed the first event if banner clicking is enabled."
  );

  if (expectEventDetected || expectEventHandled) {
    is(
      firstEvent.type,
      "cookiebannerdetected",
      "Detected event should be dispatched first"
    );
  }

  is(
    eventObservedDetected,
    expectEventDetected,
    `Should ${
      expectEventDetected ? "" : "not "
    }have observed 'cookiebannerdetected' event for mode ${mode}`
  );
  is(
    eventObservedHandled,
    expectEventHandled,
    `Should ${
      expectEventHandled ? "" : "not "
    }have observed 'cookiebannerhandled' event for mode ${mode}`
  );

  // Clean up pending promises by dispatching artificial cookiebanner events.
  // Otherwise the test fails because of pending event listeners which
  // BrowserTestUtils.waitForEvent registered.
  for (let eventType of ["cookiebannerdetected", "cookiebannerhandled"]) {
    let event = new CustomEvent(eventType, {
      bubbles: true,
      cancelable: false,
    });
    window.windowUtils.dispatchEventToChromeOnly(window, event);
  }

  await promiseEventDetected;
  await promiseEventHandled;
}
