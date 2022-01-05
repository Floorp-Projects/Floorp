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

function waitForWhile() {
  return new Promise(resolve => {
    requestIdleCallback(resolve, { timeout: 300 });
  });
}

const NativePanHandlerForWindows = {
  beginPhase: SpecialPowers.DOMWindowUtils.PHASE_BEGIN,
  updatePhase: SpecialPowers.DOMWindowUtils.PHASE_UPDATE,
  endPhase: SpecialPowers.DOMWindowUtils.PHASE_END,
  promiseNativePanEvent: promiseNativeTouchpadPan,
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

async function panRightToLeft(aElement, aX, aY) {
  await NativePanHandler.promiseNativePanEvent(
    aElement,
    aX,
    aY,
    NativePanHandler.deltaOnRTL,
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
    NativePanHandler.deltaOnRTL,
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

async function panLeftToRight(aElement, aX, aY) {
  await NativePanHandler.promiseNativePanEvent(
    aElement,
    aX,
    aY,
    -NativePanHandler.deltaOnRTL,
    0,
    NativePanHandler.beginPhase
  );
  await NativePanHandler.promiseNativePanEvent(
    aElement,
    aX,
    aY,
    -NativePanHandler.deltaOnRTL,
    0,
    NativePanHandler.updatePhase
  );
  await NativePanHandler.promiseNativePanEvent(
    aElement,
    aX,
    aY,
    -NativePanHandler.deltaOnRTL,
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

add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.eight", "Browser:ForwardOrForwardDuplicate"],
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
  await panRightToLeft(tab.linkedBrowser, 100, 100);
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
  await panLeftToRight(tab.linkedBrowser, 100, 100);
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
  await panRightToLeft(tab.linkedBrowser, 100, 100);
  is(wheelEventCount, 1, "Received a wheel event");

  await Promise.all([startLoadingPromise, stoppedLoadingPromise]);

  ok(gBrowser.webNavigation.canGoBack);

  // Now try to navigate backward again but with preventDefault-ed event
  // handler.
  wheelEventCount = 0;
  tab.linkedBrowser.addEventListener("wheel", event => {
    event.preventDefault();
  });
  await panLeftToRight(tab.linkedBrowser, 100, 100);
  is(wheelEventCount, 3, "Received all wheel events");

  await waitForWhile();
  // Make sure any navigation didn't happen.
  is(tab.linkedBrowser.currentURI.spec, secondPage);

  BrowserTestUtils.removeTab(tab);
});
