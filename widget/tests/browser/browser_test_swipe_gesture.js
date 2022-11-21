/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* import-globals-from ../../..//gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_utils.js",
  this
);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js",
  this
);

async function waitForWhile() {
  await new Promise(resolve => {
    requestIdleCallback(resolve, { timeout: 300 });
  });
  await new Promise(r => requestAnimationFrame(r));
}

const NativePanHandlerForLinux = {
  beginPhase: SpecialPowers.DOMWindowUtils.PHASE_BEGIN,
  updatePhase: SpecialPowers.DOMWindowUtils.PHASE_UPDATE,
  endPhase: SpecialPowers.DOMWindowUtils.PHASE_END,
  promiseNativePanEvent: promiseNativeTouchpadPanEventAndWaitForObserver,
  deltaOnRTL: -50,
};

const NativePanHandlerForWindows = {
  beginPhase: SpecialPowers.DOMWindowUtils.PHASE_BEGIN,
  updatePhase: SpecialPowers.DOMWindowUtils.PHASE_UPDATE,
  endPhase: SpecialPowers.DOMWindowUtils.PHASE_END,
  promiseNativePanEvent: promiseNativeTouchpadPanEventAndWaitForObserver,
  deltaOnRTL: 50,
};

const NativePanHandlerForMac = {
  // From https://developer.apple.com/documentation/coregraphics/cgscrollphase/kcgscrollphasebegan?language=occ , etc.
  beginPhase: 1, // kCGScrollPhaseBegan
  updatePhase: 2, // kCGScrollPhaseChanged
  endPhase: 4, // kCGScrollPhaseEnded
  promiseNativePanEvent: promiseNativePanGestureEventAndWaitForObserver,
  deltaOnRTL: -50,
};

function getPanHandler() {
  switch (getPlatform()) {
    case "linux":
      return NativePanHandlerForLinux;
    case "windows":
      return NativePanHandlerForWindows;
    case "mac":
      return NativePanHandlerForMac;
    default:
      throw new Error(
        "There's no native pan handler on platform " + getPlatform()
      );
  }
}

const NativePanHandler = getPanHandler();

async function panRightToLeft(aElement, aX, aY, aMultiplier) {
  await NativePanHandler.promiseNativePanEvent(
    aElement,
    aX,
    aY,
    NativePanHandler.deltaOnRTL * aMultiplier,
    0,
    NativePanHandler.beginPhase
  );
  await NativePanHandler.promiseNativePanEvent(
    aElement,
    aX,
    aY,
    NativePanHandler.deltaOnRTL,
    0,
    NativePanHandler.updatePhase
  );
  await NativePanHandler.promiseNativePanEvent(
    aElement,
    aX,
    aY,
    NativePanHandler.deltaOnRTL * aMultiplier,
    0,
    NativePanHandler.updatePhase
  );
  await NativePanHandler.promiseNativePanEvent(
    aElement,
    aX,
    aY,
    0,
    0,
    NativePanHandler.endPhase
  );
}

async function panLeftToRight(aElement, aX, aY, aMultiplier) {
  await panLeftToRightBegin(aElement, aX, aY, aMultiplier);
  await panLeftToRightUpdate(aElement, aX, aY, aMultiplier);
  await panLeftToRightEnd(aElement, aX, aY, aMultiplier);
}

async function panLeftToRightBegin(aElement, aX, aY, aMultiplier) {
  await NativePanHandler.promiseNativePanEvent(
    aElement,
    aX,
    aY,
    -NativePanHandler.deltaOnRTL * aMultiplier,
    0,
    NativePanHandler.beginPhase
  );
}

async function panLeftToRightUpdate(aElement, aX, aY, aMultiplier) {
  await NativePanHandler.promiseNativePanEvent(
    aElement,
    aX,
    aY,
    -NativePanHandler.deltaOnRTL * aMultiplier,
    0,
    NativePanHandler.updatePhase
  );
  await NativePanHandler.promiseNativePanEvent(
    aElement,
    aX,
    aY,
    -NativePanHandler.deltaOnRTL * aMultiplier,
    0,
    NativePanHandler.updatePhase
  );
}

async function panLeftToRightEnd(aElement, aX, aY, aMultiplier) {
  await NativePanHandler.promiseNativePanEvent(
    aElement,
    aX,
    aY,
    0,
    0,
    NativePanHandler.endPhase
  );
}

requestLongerTimeout(2);

add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.eight", "Browser:ForwardOrForwardDuplicate"],
      ["widget.disable-swipe-tracker", false],
      ["widget.swipe.velocity-twitch-tolerance", 0.0000001],
      ["widget.swipe.success-velocity-contribution", 0.5],
    ],
  });

  const firstPage = "about:about";
  const secondPage = "about:mozilla";
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    firstPage,
    true /* waitForLoad */
  );

  BrowserTestUtils.loadURI(tab.linkedBrowser, secondPage);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, secondPage);

  // Make sure we can go back to the previous page.
  ok(gBrowser.webNavigation.canGoBack);
  // and we cannot go forward to the next page.
  ok(!gBrowser.webNavigation.canGoForward);

  let wheelEventCount = 0;
  tab.linkedBrowser.addEventListener("wheel", () => {
    wheelEventCount++;
  });

  // Try to navigate forward.
  await panRightToLeft(tab.linkedBrowser, 100, 100, 1);
  // NOTE: The last endPhase shouldn't fire a wheel event since
  // its delta is zero.
  is(wheelEventCount, 3, "Received 3 wheel events");

  await waitForWhile();
  // Make sure any navigation didn't happen.
  is(tab.linkedBrowser.currentURI.spec, secondPage);

  // Try to navigate backward.
  wheelEventCount = 0;
  let startLoadingPromise = BrowserTestUtils.browserStarted(
    tab.linkedBrowser,
    firstPage
  );
  let stoppedLoadingPromise = BrowserTestUtils.browserStopped(
    tab.linkedBrowser,
    firstPage
  );
  await panLeftToRight(tab.linkedBrowser, 100, 100, 1);
  // NOTE: We only get a wheel event for the beginPhase, rest of events have
  // been captured by the swipe gesture module.
  is(wheelEventCount, 1, "Received a wheel event");

  // Make sure the gesture triggered going back to the previous page.
  await Promise.all([startLoadingPromise, stoppedLoadingPromise]);

  ok(gBrowser.webNavigation.canGoForward);

  // Now try to navigate forward again.
  wheelEventCount = 0;
  startLoadingPromise = BrowserTestUtils.browserStarted(
    tab.linkedBrowser,
    secondPage
  );
  stoppedLoadingPromise = BrowserTestUtils.browserStopped(
    tab.linkedBrowser,
    secondPage
  );
  await panRightToLeft(tab.linkedBrowser, 100, 100, 1);
  is(wheelEventCount, 1, "Received a wheel event");

  await Promise.all([startLoadingPromise, stoppedLoadingPromise]);

  ok(gBrowser.webNavigation.canGoBack);

  // Now try to navigate backward again but with preventDefault-ed event
  // handler.
  wheelEventCount = 0;
  let wheelEventListener = event => {
    event.preventDefault();
  };
  tab.linkedBrowser.addEventListener("wheel", wheelEventListener);
  await panLeftToRight(tab.linkedBrowser, 100, 100, 1);
  is(wheelEventCount, 3, "Received all wheel events");

  await waitForWhile();
  // Make sure any navigation didn't happen.
  is(tab.linkedBrowser.currentURI.spec, secondPage);

  // Now drop the event handler and disable the swipe tracker and try to swipe
  // again.
  wheelEventCount = 0;
  tab.linkedBrowser.removeEventListener("wheel", wheelEventListener);
  await SpecialPowers.pushPrefEnv({
    set: [["widget.disable-swipe-tracker", true]],
  });

  await panLeftToRight(tab.linkedBrowser, 100, 100, 1);
  is(wheelEventCount, 3, "Received all wheel events");

  await waitForWhile();
  // Make sure any navigation didn't happen.
  is(tab.linkedBrowser.currentURI.spec, secondPage);

  BrowserTestUtils.removeTab(tab);
});

add_task(async () => {
  // Set the default values for an OS that supports swipe to nav, except for
  // whole-page-pixel-size which varies by OS, we vary it in differente tests
  // in this file.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.eight", "Browser:ForwardOrForwardDuplicate"],
      ["browser.swipe.navigation-icon-move-distance", 0],
      ["widget.disable-swipe-tracker", false],
      ["widget.swipe.velocity-twitch-tolerance", 0.0000001],
      // Set the velocity-contribution to 0 so we can exactly control the
      // values in the swipe tracker via the delta in the events that we send.
      ["widget.swipe.success-velocity-contribution", 0.0],
      ["widget.swipe.whole-page-pixel-size", 550.0],
    ],
  });

  const firstPage = "about:about";
  const secondPage = "about:mozilla";
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    firstPage,
    true /* waitForLoad */
  );

  BrowserTestUtils.loadURI(tab.linkedBrowser, secondPage);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, secondPage);

  // Make sure we can go back to the previous page.
  ok(gBrowser.webNavigation.canGoBack);
  // and we cannot go forward to the next page.
  ok(!gBrowser.webNavigation.canGoForward);

  let wheelEventCount = 0;
  tab.linkedBrowser.addEventListener("wheel", () => {
    wheelEventCount++;
  });

  // Send a pan that starts a navigate back but doesn't have enough delta to do
  // anything. Don't send the pan end because we want to check the opacity
  // before the MSD animation in SwipeTracker starts which can temporarily put
  // us at 1 opacity.
  await panLeftToRightBegin(tab.linkedBrowser, 100, 100, 0.9);
  await panLeftToRightUpdate(tab.linkedBrowser, 100, 100, 0.9);

  // Check both getComputedStyle instead of element.style.opacity because we use a transition on the opacity.
  let computedOpacity = window
    .getComputedStyle(gHistorySwipeAnimation._prevBox)
    .getPropertyValue("opacity");
  ok(
    0.98 < computedOpacity && computedOpacity < 0.99,
    "opacity of prevbox is not quite 1"
  );
  let opacity = gHistorySwipeAnimation._prevBox.style.opacity;
  ok(0.98 < opacity && opacity < 0.99, "opacity of prevbox is not quite 1");

  const translateDistance = Services.prefs.getIntPref(
    "browser.swipe.navigation-icon-move-distance",
    0
  );
  if (translateDistance != 0) {
    isnot(
      window
        .getComputedStyle(gHistorySwipeAnimation._prevBox)
        .getPropertyValue("translate"),
      "none",
      "translate of prevbox is not `none` during gestures"
    );
  }

  await panLeftToRightEnd(tab.linkedBrowser, 100, 100, 0.9);

  // NOTE: We only get a wheel event for the beginPhase, rest of events have
  // been captured by the swipe gesture module.
  is(wheelEventCount, 1, "Received a wheel event");

  await waitForWhile();
  // Make sure any navigation didn't happen.
  is(tab.linkedBrowser.currentURI.spec, secondPage);

  // Try to navigate backward.
  wheelEventCount = 0;
  let startLoadingPromise = BrowserTestUtils.browserStarted(
    tab.linkedBrowser,
    firstPage
  );
  let stoppedLoadingPromise = BrowserTestUtils.browserStopped(
    tab.linkedBrowser,
    firstPage
  );
  await panLeftToRight(tab.linkedBrowser, 100, 100, 1);
  // NOTE: We only get a wheel event for the beginPhase, rest of events have
  // been captured by the swipe gesture module.
  is(wheelEventCount, 1, "Received a wheel event");

  // The element.style opacity will be 0 because we set it to 0 on successful navigation, however
  // we have a tranisition on it so the computed style opacity will still be 1 because the transition hasn't started yet.
  computedOpacity = window
    .getComputedStyle(gHistorySwipeAnimation._prevBox)
    .getPropertyValue("opacity");
  ok(computedOpacity == 1, "computed opacity of prevbox is 1");
  opacity = gHistorySwipeAnimation._prevBox.style.opacity;
  ok(opacity == 0, "element.style opacity of prevbox 0");

  if (translateDistance != 0) {
    // We don't have a transition for translate property so that we still have
    // some amount of translate.
    isnot(
      window
        .getComputedStyle(gHistorySwipeAnimation._prevBox)
        .getPropertyValue("translate"),
      "none",
      "translate of prevbox is not `none` during the opacity transition"
    );
  }

  // Make sure the gesture triggered going back to the previous page.
  await Promise.all([startLoadingPromise, stoppedLoadingPromise]);

  ok(gBrowser.webNavigation.canGoForward);

  BrowserTestUtils.removeTab(tab);
});

// Same test as above but whole-page-pixel-size is increased and the multipliers passed to panLeftToRight correspondingly increased.
add_task(async () => {
  // Set the default values for an OS that supports swipe to nav, except for
  // whole-page-pixel-size which varies by OS, we vary it in differente tests
  // in this file.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.eight", "Browser:ForwardOrForwardDuplicate"],
      ["widget.disable-swipe-tracker", false],
      ["widget.swipe.velocity-twitch-tolerance", 0.0000001],
      // Set the velocity-contribution to 0 so we can exactly control the
      // values in the swipe tracker via the delta in the events that we send.
      ["widget.swipe.success-velocity-contribution", 0.0],
      ["widget.swipe.whole-page-pixel-size", 1100.0],
    ],
  });

  const firstPage = "about:about";
  const secondPage = "about:mozilla";
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    firstPage,
    true /* waitForLoad */
  );

  BrowserTestUtils.loadURI(tab.linkedBrowser, secondPage);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, secondPage);

  // Make sure we can go back to the previous page.
  ok(gBrowser.webNavigation.canGoBack);
  // and we cannot go forward to the next page.
  ok(!gBrowser.webNavigation.canGoForward);

  let wheelEventCount = 0;
  tab.linkedBrowser.addEventListener("wheel", () => {
    wheelEventCount++;
  });

  // Send a pan that starts a navigate back but doesn't have enough delta to do
  // anything. Don't send the pan end because we want to check the opacity
  // before the MSD animation in SwipeTracker starts which can temporarily put
  // us at 1 opacity.
  await panLeftToRightBegin(tab.linkedBrowser, 100, 100, 1.8);
  await panLeftToRightUpdate(tab.linkedBrowser, 100, 100, 1.8);

  // Check both getComputedStyle instead of element.style.opacity because we use a transition on the opacity.
  let computedOpacity = window
    .getComputedStyle(gHistorySwipeAnimation._prevBox)
    .getPropertyValue("opacity");
  ok(
    0.98 < computedOpacity && computedOpacity < 0.99,
    "opacity of prevbox is not quite 1"
  );
  let opacity = gHistorySwipeAnimation._prevBox.style.opacity;
  ok(0.98 < opacity && opacity < 0.99, "opacity of prevbox is not quite 1");

  await panLeftToRightEnd(tab.linkedBrowser, 100, 100, 1.8);

  // NOTE: We only get a wheel event for the beginPhase, rest of events have
  // been captured by the swipe gesture module.
  is(wheelEventCount, 1, "Received a wheel event");

  await waitForWhile();
  // Make sure any navigation didn't happen.
  is(tab.linkedBrowser.currentURI.spec, secondPage);

  // Try to navigate backward.
  wheelEventCount = 0;
  let startLoadingPromise = BrowserTestUtils.browserStarted(
    tab.linkedBrowser,
    firstPage
  );
  let stoppedLoadingPromise = BrowserTestUtils.browserStopped(
    tab.linkedBrowser,
    firstPage
  );
  await panLeftToRight(tab.linkedBrowser, 100, 100, 2);
  // NOTE: We only get a wheel event for the beginPhase, rest of events have
  // been captured by the swipe gesture module.
  is(wheelEventCount, 1, "Received a wheel event");

  // The element.style opacity will be 0 because we set it to 0 on successful navigation, however
  // we have a tranisition on it so the computed style opacity will still be 1 because the transition hasn't started yet.
  computedOpacity = window
    .getComputedStyle(gHistorySwipeAnimation._prevBox)
    .getPropertyValue("opacity");
  ok(computedOpacity == 1, "computed opacity of prevbox is 1");
  opacity = gHistorySwipeAnimation._prevBox.style.opacity;
  ok(opacity == 0, "element.style opacity of prevbox 0");

  // Make sure the gesture triggered going back to the previous page.
  await Promise.all([startLoadingPromise, stoppedLoadingPromise]);

  ok(gBrowser.webNavigation.canGoForward);

  BrowserTestUtils.removeTab(tab);
});

add_task(async () => {
  // Set the default values for an OS that supports swipe to nav, except for
  // whole-page-pixel-size which varies by OS, we vary it in different tests
  // in this file.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.eight", "Browser:ForwardOrForwardDuplicate"],
      ["widget.disable-swipe-tracker", false],
      ["widget.swipe.velocity-twitch-tolerance", 0.0000001],
      // Set the velocity-contribution to 1 (default 0.05f) so velocity is a
      // large contribution to the success value in SwipeTracker.cpp so it
      // pushes us into success territory without going into success territory
      // purely from th deltas.
      ["widget.swipe.success-velocity-contribution", 2.0],
      ["widget.swipe.whole-page-pixel-size", 550.0],
    ],
  });

  async function runTest() {
    const firstPage = "about:about";
    const secondPage = "about:mozilla";
    const tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      firstPage,
      true /* waitForLoad */
    );

    BrowserTestUtils.loadURI(tab.linkedBrowser, secondPage);
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, secondPage);

    // Make sure we can go back to the previous page.
    ok(gBrowser.webNavigation.canGoBack);
    // and we cannot go forward to the next page.
    ok(!gBrowser.webNavigation.canGoForward);

    let wheelEventCount = 0;
    tab.linkedBrowser.addEventListener("wheel", () => {
      wheelEventCount++;
    });

    let startLoadingPromise = BrowserTestUtils.browserStarted(
      tab.linkedBrowser,
      firstPage
    );
    let stoppedLoadingPromise = BrowserTestUtils.browserStopped(
      tab.linkedBrowser,
      firstPage
    );
    let startTime = performance.now();
    await panLeftToRight(tab.linkedBrowser, 100, 100, 0.2);
    let endTime = performance.now();

    // If sending the events took too long then we might not have been able
    // to generate enough velocity.
    // The value 230 was picked based on try runs, in particular test verify
    // runs on mac were the long pole, and when we get times near this we can
    // still achieve the required velocity.
    if (endTime - startTime > 230) {
      BrowserTestUtils.removeTab(tab);
      return false;
    }

    // NOTE: We only get a wheel event for the beginPhase, rest of events have
    // been captured by the swipe gesture module.
    is(wheelEventCount, 1, "Received a wheel event");

    // The element.style opacity will be 0 because we set it to 0 on successful navigation, however
    // we have a tranisition on it so the computed style opacity will still be 1 because the transition hasn't started yet.
    let computedOpacity = window
      .getComputedStyle(gHistorySwipeAnimation._prevBox)
      .getPropertyValue("opacity");
    ok(computedOpacity == 1, "computed opacity of prevbox is 1");
    let opacity = gHistorySwipeAnimation._prevBox.style.opacity;
    ok(opacity == 0, "element.style opacity of prevbox 0");

    // Make sure the gesture triggered going back to the previous page.
    await Promise.all([startLoadingPromise, stoppedLoadingPromise]);

    ok(gBrowser.webNavigation.canGoForward);

    BrowserTestUtils.removeTab(tab);

    return true;
  }

  let numTries = 15;
  while (numTries > 0) {
    await new Promise(r => requestAnimationFrame(r));
    await new Promise(resolve => requestIdleCallback(resolve));
    await new Promise(r => requestAnimationFrame(r));

    // runTest return value indicates if test was able to run to the end.
    if (await runTest()) {
      break;
    }
    numTries--;
  }
  ok(numTries > 0, "never ran the test");
});

add_task(async () => {
  // Set the default values for an OS that supports swipe to nav, except for
  // whole-page-pixel-size which varies by OS, we vary it in differente tests
  // in this file.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.eight", "Browser:ForwardOrForwardDuplicate"],
      ["widget.disable-swipe-tracker", false],
      ["widget.swipe.velocity-twitch-tolerance", 0.0000001],
      // Set the velocity-contribution to 0 so we can exactly control the
      // values in the swipe tracker via the delta in the events that we send.
      ["widget.swipe.success-velocity-contribution", 0.0],
      ["widget.swipe.whole-page-pixel-size", 550.0],
    ],
  });

  const firstPage = "about:about";
  const secondPage = "about:mozilla";
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    firstPage,
    true /* waitForLoad */
  );

  BrowserTestUtils.loadURI(tab.linkedBrowser, secondPage);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, secondPage);

  // Make sure we can go back to the previous page.
  ok(gBrowser.webNavigation.canGoBack);
  // and we cannot go forward to the next page.
  ok(!gBrowser.webNavigation.canGoForward);

  let startLoadingPromise = BrowserTestUtils.browserStarted(
    tab.linkedBrowser,
    firstPage
  );
  let stoppedLoadingPromise = BrowserTestUtils.browserStopped(
    tab.linkedBrowser,
    firstPage
  );
  await panLeftToRight(tab.linkedBrowser, 100, 100, 2);

  // Make sure the gesture triggered going back to the previous page.
  await Promise.all([startLoadingPromise, stoppedLoadingPromise]);

  ok(gBrowser.webNavigation.canGoForward);

  while (
    gHistorySwipeAnimation._prevBox != null ||
    gHistorySwipeAnimation._nextBox != null
  ) {
    await new Promise(r => requestAnimationFrame(r));
  }

  ok(
    gHistorySwipeAnimation._prevBox == null &&
      gHistorySwipeAnimation._nextBox == null
  );

  BrowserTestUtils.removeTab(tab);
});

add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.eight", "Browser:ForwardOrForwardDuplicate"],
      ["widget.disable-swipe-tracker", false],
      ["widget.swipe.velocity-twitch-tolerance", 0.0000001],
      // Set the velocity-contribution to 0 so we can exactly control the
      // values in the swipe tracker via the delta in the events that we send.
      ["widget.swipe.success-velocity-contribution", 0.0],
      ["widget.swipe.whole-page-pixel-size", 550.0],
    ],
  });

  function swipeGestureEndPromise() {
    return new Promise(resolve => {
      let promiseObserver = {
        handleEvent(aEvent) {
          switch (aEvent.type) {
            case "MozSwipeGestureEnd":
              gBrowser.tabbox.removeEventListener(
                "MozSwipeGestureEnd",
                promiseObserver,
                true
              );
              resolve();
              break;
          }
        },
      };
      gBrowser.tabbox.addEventListener(
        "MozSwipeGestureEnd",
        promiseObserver,
        true
      );
    });
  }

  const firstPage = "about:about";
  const secondPage = "about:mozilla";
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    firstPage,
    true /* waitForLoad */
  );

  BrowserTestUtils.loadURI(tab.linkedBrowser, secondPage);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, secondPage);

  // Make sure we can go back to the previous page.
  ok(gBrowser.webNavigation.canGoBack);
  // and we cannot go forward to the next page.
  ok(!gBrowser.webNavigation.canGoForward);

  let numSwipeGestureEndEvents = 0;
  var anObserver = {
    handleEvent(aEvent) {
      switch (aEvent.type) {
        case "MozSwipeGestureEnd":
          numSwipeGestureEndEvents++;
          break;
      }
    },
  };

  gBrowser.tabbox.addEventListener("MozSwipeGestureEnd", anObserver, true);

  let gestureEndPromise = swipeGestureEndPromise();

  is(
    numSwipeGestureEndEvents,
    0,
    "expected no MozSwipeGestureEnd got " + numSwipeGestureEndEvents
  );

  // Send a pan that starts a navigate back but doesn't have enough delta to do
  // anything.
  await panLeftToRight(tab.linkedBrowser, 100, 100, 0.9);

  await waitForWhile();
  // Make sure any navigation didn't happen.
  is(tab.linkedBrowser.currentURI.spec, secondPage);
  // end event comes after a swipe that does not navigate
  await gestureEndPromise;
  is(
    numSwipeGestureEndEvents,
    1,
    "expected one MozSwipeGestureEnd got " + numSwipeGestureEndEvents
  );

  // Try to navigate backward.
  let startLoadingPromise = BrowserTestUtils.browserStarted(
    tab.linkedBrowser,
    firstPage
  );
  let stoppedLoadingPromise = BrowserTestUtils.browserStopped(
    tab.linkedBrowser,
    firstPage
  );

  gestureEndPromise = swipeGestureEndPromise();

  await panLeftToRight(tab.linkedBrowser, 100, 100, 1);

  // Make sure the gesture triggered going back to the previous page.
  await Promise.all([startLoadingPromise, stoppedLoadingPromise]);

  ok(gBrowser.webNavigation.canGoForward);

  await gestureEndPromise;

  is(
    numSwipeGestureEndEvents,
    2,
    "expected one MozSwipeGestureEnd got " + (numSwipeGestureEndEvents - 1)
  );

  gBrowser.tabbox.removeEventListener("MozSwipeGestureEnd", anObserver, true);

  BrowserTestUtils.removeTab(tab);
});

add_task(async () => {
  // success-velocity-contribution is very high and whole-page-pixel-size is
  // very low so that one swipe goes over the threshold asap.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.eight", "Browser:ForwardOrForwardDuplicate"],
      ["widget.disable-swipe-tracker", false],
      ["widget.swipe.velocity-twitch-tolerance", 0.0000001],
      ["widget.swipe.success-velocity-contribution", 999999.0],
      ["widget.swipe.whole-page-pixel-size", 1.0],
    ],
  });

  const firstPage = "about:about";
  const secondPage = "about:mozilla";
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    firstPage,
    true /* waitForLoad */
  );

  BrowserTestUtils.loadURI(tab.linkedBrowser, secondPage);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, secondPage);

  // Make sure we can go back to the previous page.
  ok(gBrowser.webNavigation.canGoBack);
  // and we cannot go forward to the next page.
  ok(!gBrowser.webNavigation.canGoForward);

  // Navigate backward.
  let startLoadingPromise = BrowserTestUtils.browserStarted(
    tab.linkedBrowser,
    firstPage
  );
  let stoppedLoadingPromise = BrowserTestUtils.browserStopped(
    tab.linkedBrowser,
    firstPage
  );

  await panLeftToRightBegin(tab.linkedBrowser, 100, 100, 100);

  ok(gHistorySwipeAnimation._prevBox != null, "should have prevbox");
  let transitionPromise = new Promise(resolve => {
    gHistorySwipeAnimation._prevBox.addEventListener(
      "transitionstart",
      resolve,
      { once: true }
    );
  });

  await panLeftToRightUpdate(tab.linkedBrowser, 100, 100, 100);
  await panLeftToRightEnd(tab.linkedBrowser, 100, 100, 100);

  // Make sure the gesture triggered going back to the previous page.
  await Promise.all([startLoadingPromise, stoppedLoadingPromise]);

  ok(gBrowser.webNavigation.canGoForward);

  await transitionPromise;

  await TestUtils.waitForCondition(() => {
    return (
      gHistorySwipeAnimation._prevBox == null &&
      gHistorySwipeAnimation._nextBox == null
    );
  });

  BrowserTestUtils.removeTab(tab);
});

// A simple test case on RTL.
add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.eight", "Browser:ForwardOrForwardDuplicate"],
      ["widget.disable-swipe-tracker", false],
      ["widget.swipe.velocity-twitch-tolerance", 0.0000001],
      ["widget.swipe.success-velocity-contribution", 0.5],
      ["intl.l10n.pseudo", "bidi"],
    ],
  });

  const newWin = await BrowserTestUtils.openNewBrowserWindow();

  const firstPage = "about:about";
  const secondPage = "about:mozilla";
  const tab = await BrowserTestUtils.openNewForegroundTab(
    newWin.gBrowser,
    firstPage,
    true /* waitForLoad */
  );

  BrowserTestUtils.loadURI(tab.linkedBrowser, secondPage);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, secondPage);

  // Make sure we can go back to the previous page.
  ok(newWin.gBrowser.webNavigation.canGoBack);
  // and we cannot go forward to the next page.
  ok(!newWin.gBrowser.webNavigation.canGoForward);

  // Make sure that our gesture support stuff has been initialized in the new
  // browser window.
  await TestUtils.waitForCondition(() => {
    return newWin.gHistorySwipeAnimation.active;
  });

  // Try to navigate backward.
  let startLoadingPromise = BrowserTestUtils.browserStarted(
    tab.linkedBrowser,
    firstPage
  );
  let stoppedLoadingPromise = BrowserTestUtils.browserStopped(
    tab.linkedBrowser,
    firstPage
  );
  await panRightToLeft(tab.linkedBrowser, 100, 100, 1);
  await Promise.all([startLoadingPromise, stoppedLoadingPromise]);

  ok(newWin.gBrowser.webNavigation.canGoForward);

  // Now try to navigate forward again.
  startLoadingPromise = BrowserTestUtils.browserStarted(
    tab.linkedBrowser,
    secondPage
  );
  stoppedLoadingPromise = BrowserTestUtils.browserStopped(
    tab.linkedBrowser,
    secondPage
  );
  await panLeftToRight(tab.linkedBrowser, 100, 100, 1);
  await Promise.all([startLoadingPromise, stoppedLoadingPromise]);

  ok(newWin.gBrowser.webNavigation.canGoBack);

  await BrowserTestUtils.closeWindow(newWin);
});
