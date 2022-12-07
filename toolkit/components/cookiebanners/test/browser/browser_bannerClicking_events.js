/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(clickTestSetup);

/**
 * Triggers the cookie banner clicking feature and tests the events dispatched.
 * @param {*} options - Test options.
 * @param {nsICookieBannerService::Modes} options.mode - The cookie banner service mode to test with.
 * @param {*} options.openPageOptions - Options to overwrite for the openPageAndVerify call.
 */
async function runEventTest({ mode, openPageOptions = {} }) {
  await SpecialPowers.pushPrefEnv({
    set: [["cookiebanners.service.mode", mode]],
  });

  // Insert rules only if the feature is enabled.
  if (Services.cookieBanners.isEnabled) {
    insertTestClickRules();
  }

  let expectEventDetected = mode != Ci.nsICookieBannerService.MODE_DISABLED;
  let expectEventHandled =
    mode == Ci.nsICookieBannerService.MODE_REJECT ||
    mode == Ci.nsICookieBannerService.MODE_REJECT_OR_ACCEPT;

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

  let shouldHandleBanner = mode == Ci.nsICookieBannerService.MODE_REJECT;
  let testURL = openPageOptions.testURL || TEST_PAGE_A;

  await openPageAndVerify({
    win: window,
    domain: TEST_DOMAIN_A,
    testURL,
    visible: !shouldHandleBanner,
    expected: shouldHandleBanner ? "OptOut" : "NoClick",
    keepTabOpen: true,
    ...openPageOptions, // Allow test callers to override any options for this method.
  });

  // It's safe to evaluate the event state at this point, because
  // openPageAndVerify waits for the cookie banner clicking code to finish with
  // a test-only observer message from CookieBannerParent before resolving.

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

  // Clean up the test tab opened by openPageAndVerify.
  BrowserTestUtils.removeTab(gBrowser.selectedTab);

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

/**
 * Test the banner clicking events with MODE_REJECT.
 */
add_task(async function test_events_mode_reject() {
  await runEventTest({ mode: Ci.nsICookieBannerService.MODE_REJECT });
});

/**
 * Test the banner clicking events with MODE_DETECT_ONLY.
 */
add_task(async function test_events_mode_detect_only() {
  await runEventTest({ mode: Ci.nsICookieBannerService.MODE_DETECT_ONLY });
});

/**
 * Test the banner clicking events with MODE_DETECT_ONLY with a click rule that
 * only supports opt-in..
 */
add_task(async function test_events_mode_detect_only_opt_in_rule() {
  await runEventTest({
    mode: Ci.nsICookieBannerService.MODE_DETECT_ONLY,
    openPageOptions: {
      // We only have an opt-in rule for DOMAIN_B. This ensures we still fire
      // detection events for that case.
      domain: TEST_DOMAIN_B,
      testURL: TEST_PAGE_B,
      shouldHandleBanner: true,
      expected: "NoClick",
    },
  });
});

/**
 * Test the banner clicking events with MODE_DETECT_ONLY.
 */
add_task(async function test_events_mode_disabled() {
  await runEventTest({ mode: Ci.nsICookieBannerService.MODE_DISABLED });
});
