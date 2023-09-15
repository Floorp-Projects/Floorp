/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

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

requestLongerTimeout(2);

add_task(async () => {
  // Set the default values for an OS that supports swipe to nav, except for
  // pixel-size which varies by OS, we vary it in differente tests in this file.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.right", "Browser:ForwardOrForwardDuplicate"],
      ["widget.disable-swipe-tracker", false],
      ["widget.swipe.velocity-twitch-tolerance", 0.0000001],
      // Set the velocity-contribution to 0 so we can exactly control the
      // values in the swipe tracker via the delta in the events that we send.
      ["widget.swipe.success-velocity-contribution", 0.0],
      ["widget.swipe.pixel-size", 550.0],
    ],
  });

  const firstPage = "about:about";
  const secondPage = "about:mozilla";
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    firstPage,
    true /* waitForLoad */
  );

  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, secondPage);
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
  is(computedOpacity, "1", "opacity of prevbox is 1");
  let opacity = gHistorySwipeAnimation._prevBox.style.opacity;
  is(opacity, "", "opacity style isn't explicitly set");

  const isTranslatingIcon =
    Services.prefs.getIntPref(
      "browser.swipe.navigation-icon-start-position",
      0
    ) != 0 ||
    Services.prefs.getIntPref(
      "browser.swipe.navigation-icon-end-position",
      0
    ) != 0;
  if (isTranslatingIcon != 0) {
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

  if (isTranslatingIcon) {
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
  await SpecialPowers.popPrefEnv();
});

// Same test as above but pixel-size is increased and the multipliers passed to panLeftToRight correspondingly increased.
add_task(async () => {
  // Set the default values for an OS that supports swipe to nav, except for
  // pixel-size which varies by OS, we vary it in differente tests
  // in this file.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.right", "Browser:ForwardOrForwardDuplicate"],
      ["widget.disable-swipe-tracker", false],
      ["widget.swipe.velocity-twitch-tolerance", 0.0000001],
      // Set the velocity-contribution to 0 so we can exactly control the
      // values in the swipe tracker via the delta in the events that we send.
      ["widget.swipe.success-velocity-contribution", 0.0],
      ["widget.swipe.pixel-size", 1100.0],
    ],
  });

  const firstPage = "about:about";
  const secondPage = "about:mozilla";
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    firstPage,
    true /* waitForLoad */
  );

  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, secondPage);
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
  is(computedOpacity, "1", "opacity of prevbox is 1");
  let opacity = gHistorySwipeAnimation._prevBox.style.opacity;
  is(opacity, "", "opacity style isn't explicitly set");

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
  await SpecialPowers.popPrefEnv();
});

add_task(async () => {
  // Set the default values for an OS that supports swipe to nav, except for
  // pixel-size which varies by OS, we vary it in different tests
  // in this file.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.right", "Browser:ForwardOrForwardDuplicate"],
      ["widget.disable-swipe-tracker", false],
      ["widget.swipe.velocity-twitch-tolerance", 0.0000001],
      // Set the velocity-contribution to 1 (default 0.05f) so velocity is a
      // large contribution to the success value in SwipeTracker.cpp so it
      // pushes us into success territory without going into success territory
      // purely from th deltas.
      ["widget.swipe.success-velocity-contribution", 2.0],
      ["widget.swipe.pixel-size", 550.0],
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

    BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, secondPage);
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
  await SpecialPowers.popPrefEnv();
});

add_task(async () => {
  // Set the default values for an OS that supports swipe to nav, except for
  // pixel-size which varies by OS, we vary it in differente tests
  // in this file.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.right", "Browser:ForwardOrForwardDuplicate"],
      ["widget.disable-swipe-tracker", false],
      ["widget.swipe.velocity-twitch-tolerance", 0.0000001],
      // Set the velocity-contribution to 0 so we can exactly control the
      // values in the swipe tracker via the delta in the events that we send.
      ["widget.swipe.success-velocity-contribution", 0.0],
      ["widget.swipe.pixel-size", 550.0],
    ],
  });

  const firstPage = "about:about";
  const secondPage = "about:mozilla";
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    firstPage,
    true /* waitForLoad */
  );

  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, secondPage);
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
  await SpecialPowers.popPrefEnv();
});

add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.right", "Browser:ForwardOrForwardDuplicate"],
      ["widget.disable-swipe-tracker", false],
      ["widget.swipe.velocity-twitch-tolerance", 0.0000001],
      // Set the velocity-contribution to 0 so we can exactly control the
      // values in the swipe tracker via the delta in the events that we send.
      ["widget.swipe.success-velocity-contribution", 0.0],
      ["widget.swipe.pixel-size", 550.0],
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

  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, secondPage);
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
  await SpecialPowers.popPrefEnv();
});

add_task(async () => {
  // success-velocity-contribution is very high and pixel-size is
  // very low so that one swipe goes over the threshold asap.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.right", "Browser:ForwardOrForwardDuplicate"],
      ["widget.disable-swipe-tracker", false],
      ["widget.swipe.velocity-twitch-tolerance", 0.0000001],
      ["widget.swipe.success-velocity-contribution", 999999.0],
      ["widget.swipe.pixel-size", 1.0],
    ],
  });

  const firstPage = "about:about";
  const secondPage = "about:mozilla";
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    firstPage,
    true /* waitForLoad */
  );

  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, secondPage);
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
  let transitionCancelPromise = new Promise(resolve => {
    gHistorySwipeAnimation._prevBox.addEventListener(
      "transitioncancel",
      event => {
        if (
          event.propertyName == "opacity" &&
          event.target == gHistorySwipeAnimation._prevBox
        ) {
          resolve();
        }
      },
      { once: true }
    );
  });
  let transitionStartPromise = new Promise(resolve => {
    gHistorySwipeAnimation._prevBox.addEventListener(
      "transitionstart",
      event => {
        if (
          event.propertyName == "opacity" &&
          event.target == gHistorySwipeAnimation._prevBox
        ) {
          resolve();
        }
      },
      { once: true }
    );
  });

  await panLeftToRightUpdate(tab.linkedBrowser, 100, 100, 100);
  await panLeftToRightEnd(tab.linkedBrowser, 100, 100, 100);

  // Make sure the gesture triggered going back to the previous page.
  await Promise.all([startLoadingPromise, stoppedLoadingPromise]);

  ok(gBrowser.webNavigation.canGoForward);

  await Promise.any([transitionStartPromise, transitionCancelPromise]);

  await TestUtils.waitForCondition(() => {
    return (
      gHistorySwipeAnimation._prevBox == null &&
      gHistorySwipeAnimation._nextBox == null
    );
  });

  // Navigate forward and check the forward navigation icon box state.
  startLoadingPromise = BrowserTestUtils.browserStarted(
    tab.linkedBrowser,
    secondPage
  );
  stoppedLoadingPromise = BrowserTestUtils.browserStopped(
    tab.linkedBrowser,
    secondPage
  );

  await panRightToLeftBegin(tab.linkedBrowser, 100, 100, 100);

  ok(gHistorySwipeAnimation._nextBox != null, "should have nextbox");
  transitionCancelPromise = new Promise(resolve => {
    gHistorySwipeAnimation._nextBox.addEventListener(
      "transitioncancel",
      event => {
        if (
          event.propertyName == "opacity" &&
          event.target == gHistorySwipeAnimation._nextBox
        ) {
          resolve();
        }
      }
    );
  });
  transitionStartPromise = new Promise(resolve => {
    gHistorySwipeAnimation._nextBox.addEventListener(
      "transitionstart",
      event => {
        if (
          event.propertyName == "opacity" &&
          event.target == gHistorySwipeAnimation._nextBox
        ) {
          resolve();
        }
      }
    );
  });

  await panRightToLeftUpdate(tab.linkedBrowser, 100, 100, 100);
  await panRightToLeftEnd(tab.linkedBrowser, 100, 100, 100);

  // Make sure the gesture triggered going forward to the next page.
  await Promise.all([startLoadingPromise, stoppedLoadingPromise]);

  ok(gBrowser.webNavigation.canGoBack);

  await Promise.any([transitionStartPromise, transitionCancelPromise]);

  await TestUtils.waitForCondition(() => {
    return (
      gHistorySwipeAnimation._nextBox == null &&
      gHistorySwipeAnimation._prevBox == null
    );
  });

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

// A simple test case on RTL.
add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.right", "Browser:ForwardOrForwardDuplicate"],
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

  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, secondPage);
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
  await SpecialPowers.popPrefEnv();
});

add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.right", "Browser:ForwardOrForwardDuplicate"],
      ["widget.disable-swipe-tracker", false],
      ["widget.swipe.velocity-twitch-tolerance", 0.0000001],
      ["widget.swipe.success-velocity-contribution", 0.5],
      ["apz.overscroll.enabled", true],
      ["apz.test.logging_enabled", true],
    ],
  });

  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:about",
    true /* waitForLoad */
  );

  const URL_ROOT = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content/",
    "http://mochi.test:8888/"
  );
  BrowserTestUtils.startLoadingURIString(
    tab.linkedBrowser,
    URL_ROOT + "helper_swipe_gesture.html"
  );
  await BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false /* includeSubFrames */,
    URL_ROOT + "helper_swipe_gesture.html"
  );

  // Make sure we can go back to the previous page.
  ok(gBrowser.webNavigation.canGoBack);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    // Set `overscroll-behavior-x: contain` and flush it.
    content.document.documentElement.style.overscrollBehaviorX = "contain";
    content.document.documentElement.getBoundingClientRect();
    await content.wrappedJSObject.promiseApzFlushedRepaints();
  });

  // Start a pan gesture but keep touching.
  await panLeftToRightBegin(tab.linkedBrowser, 100, 100, 2);

  // Flush APZ pending requests to make sure the pan gesture has been processed.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.promiseApzFlushedRepaints();
  });

  const isOverscrolled = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    () => {
      const scrollId = SpecialPowers.DOMWindowUtils.getViewId(
        content.document.scrollingElement
      );
      const data = SpecialPowers.DOMWindowUtils.getCompositorAPZTestData();
      return data.additionalData.some(entry => {
        return (
          entry.key == scrollId &&
          entry.value.split(",").includes("overscrolled")
        );
      });
    }
  );

  ok(isOverscrolled, "The root scroller should have overscrolled");

  // Finish the pan gesture.
  await panLeftToRightUpdate(tab.linkedBrowser, 100, 100, 2);
  await panLeftToRightEnd(tab.linkedBrowser, 100, 100, 2);

  // And wait a while to give a chance to navigate.
  await waitForWhile();

  // Make sure any navigation didn't happen.
  is(tab.linkedBrowser.currentURI.spec, URL_ROOT + "helper_swipe_gesture.html");

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

// A test case to make sure the short circuit path for swipe-to-navigations in
// APZ works, i.e. cases where we know for sure that the target APZC for a given
// pan-start event isn't scrollable in the pan-start event direction.
add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.right", "Browser:ForwardOrForwardDuplicate"],
      ["widget.disable-swipe-tracker", false],
      ["apz.overscroll.enabled", true],
    ],
  });

  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:about",
    true /* waitForLoad */
  );

  const URL_ROOT = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content/",
    "http://mochi.test:8888/"
  );
  BrowserTestUtils.startLoadingURIString(
    tab.linkedBrowser,
    URL_ROOT + "helper_swipe_gesture.html"
  );
  await BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false /* includeSubFrames */,
    URL_ROOT + "helper_swipe_gesture.html"
  );

  // Make sure the content can allow both of overscrolling and
  // swipe-to-navigations.
  const overscrollBehaviorX = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    () => {
      return content.window.getComputedStyle(content.document.documentElement)
        .overscrollBehaviorX;
    }
  );
  is(overscrollBehaviorX, "auto");

  // Make sure we can go back to the previous page.
  ok(gBrowser.webNavigation.canGoBack);

  // Start a pan gesture but keep touching.
  await panLeftToRightBegin(tab.linkedBrowser, 100, 100, 2);

  // The above pan event should invoke a SwipeGestureStart event immediately so
  // that the swipe-to-navigation icon box should be uncollapsed to show it.
  ok(!gHistorySwipeAnimation._prevBox.collapsed);

  // Finish the pan gesture, i.e. sending a pan-end event, otherwise a new
  // pan-start event in the next will also generate a pan-interrupt event which
  // will break the test.
  await panLeftToRightUpdate(tab.linkedBrowser, 100, 100, 2);
  await panLeftToRightEnd(tab.linkedBrowser, 100, 100, 2);

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.right", "Browser:ForwardOrForwardDuplicate"],
      ["widget.disable-swipe-tracker", false],
      ["widget.swipe.velocity-twitch-tolerance", 0.0000001],
      ["widget.swipe.success-velocity-contribution", 0.5],
      ["apz.overscroll.enabled", true],
      ["apz.test.logging_enabled", true],
    ],
  });

  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:about",
    true /* waitForLoad */
  );

  const URL_ROOT = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content/",
    "http://mochi.test:8888/"
  );
  BrowserTestUtils.startLoadingURIString(
    tab.linkedBrowser,
    URL_ROOT + "helper_swipe_gesture.html"
  );
  await BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false /* includeSubFrames */,
    URL_ROOT + "helper_swipe_gesture.html"
  );

  // Make sure we can go back to the previous page.
  ok(gBrowser.webNavigation.canGoBack);

  // Start a pan gesture but keep touching.
  await panLeftToRightBegin(tab.linkedBrowser, 100, 100, 2);

  // Flush APZ pending requests to make sure the pan gesture has been processed.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    await content.wrappedJSObject.promiseApzFlushedRepaints();
  });

  const isOverscrolled = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    () => {
      const scrollId = SpecialPowers.DOMWindowUtils.getViewId(
        content.document.scrollingElement
      );
      const data = SpecialPowers.DOMWindowUtils.getCompositorAPZTestData();
      return data.additionalData.some(entry => {
        return entry.key == scrollId && entry.value.includes("overscrolled");
      });
    }
  );

  ok(!isOverscrolled, "The root scroller should not have overscrolled");

  await panLeftToRightEnd(tab.linkedBrowser, 100, 100, 0);

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.right", "Browser:ForwardOrForwardDuplicate"],
      ["widget.disable-swipe-tracker", false],
      ["widget.swipe.velocity-twitch-tolerance", 0.0000001],
      ["widget.swipe.success-velocity-contribution", 0.5],
    ],
  });

  // Load three pages and go to the second page so that it can be navigated
  // to both back and forward.
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:about",
    true /* waitForLoad */
  );

  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, "about:mozilla");
  await BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false /* includeSubFrames */,
    "about:mozilla"
  );

  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, "about:home");
  await BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false /* includeSubFrames */,
    "about:home"
  );

  gBrowser.goBack();
  await BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false /* includeSubFrames */,
    "about:mozilla"
  );

  // Make sure we can go back and go forward.
  ok(gBrowser.webNavigation.canGoBack);
  ok(gBrowser.webNavigation.canGoForward);

  // Start a history back pan gesture but keep touching.
  await panLeftToRightBegin(tab.linkedBrowser, 100, 100, 1);

  ok(
    !gHistorySwipeAnimation._prevBox.collapsed,
    "The icon box for the previous navigation should NOT be collapsed"
  );
  ok(
    gHistorySwipeAnimation._nextBox.collapsed,
    "The icon box for the next navigation should be collapsed"
  );

  // Pan back to the opposite direction so that the gesture should be cancelled.
  // eslint-disable-next-line no-undef
  await NativePanHandler.promiseNativePanEvent(
    tab.linkedBrowser,
    100,
    100,
    // eslint-disable-next-line no-undef
    NativePanHandler.delta,
    0,
    // eslint-disable-next-line no-undef
    NativePanHandler.updatePhase
  );

  ok(
    gHistorySwipeAnimation._prevBox.collapsed,
    "The icon box for the previous navigation should be collapsed"
  );
  ok(
    gHistorySwipeAnimation._nextBox.collapsed,
    "The icon box for the next navigation should be collapsed"
  );

  await panLeftToRightEnd(tab.linkedBrowser, 100, 100, 0);

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.right", "Browser:ForwardOrForwardDuplicate"],
      ["widget.disable-swipe-tracker", false],
      ["widget.swipe.velocity-twitch-tolerance", 0.0000001],
      ["widget.swipe.success-velocity-contribution", 0.5],
      ["apz.overscroll.enabled", true],
      ["apz.overscroll.damping", 5.0],
      ["apz.content_response_timeout", 0],
    ],
  });

  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:about",
    true /* waitForLoad */
  );

  const URL_ROOT = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content/",
    "http://mochi.test:8888/"
  );

  // Load a horizontal scrollable content.
  BrowserTestUtils.startLoadingURIString(
    tab.linkedBrowser,
    URL_ROOT + "helper_swipe_gesture.html"
  );
  await BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false /* includeSubFrames */,
    URL_ROOT + "helper_swipe_gesture.html"
  );

  // Make sure we can go back to the previous page.
  ok(gBrowser.webNavigation.canGoBack);

  // Shift the horizontal scroll position slightly to make the content
  // overscrollable.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    content.document.documentElement.scrollLeft = 1;
    content.document.documentElement.getBoundingClientRect();
    await content.wrappedJSObject.promiseApzFlushedRepaints();
  });

  // Swipe horizontally to overscroll.
  await panLeftToRight(tab.linkedBrowser, 1, 100, 1);

  // Swipe again over the overscroll gutter.
  await panLeftToRight(tab.linkedBrowser, 1, 100, 1);

  // Wait the overscroll gutter is restored.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    // For some reasons using functions in apz_test_native_event_utils.js
    // sometimes causes "TypeError content.wrappedJSObject.XXXX is not a
    // function" error, so we observe "APZ:TransformEnd" instead of using
    // promiseTransformEnd().
    await new Promise((resolve, reject) => {
      SpecialPowers.Services.obs.addObserver(function observer(
        subject,
        topic,
        data
      ) {
        try {
          SpecialPowers.Services.obs.removeObserver(observer, topic);
          resolve([subject, data]);
        } catch (ex) {
          SpecialPowers.Services.obs.removeObserver(observer, topic);
          reject(ex);
        }
      },
      "APZ:TransformEnd");
    });
  });

  // Set up an APZ aware event listener and...
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    content.document.documentElement.addEventListener("wheel", e => {}, {
      passive: false,
    });
    await content.wrappedJSObject.promiseApzFlushedRepaints();
  });

  // Try to swipe back again without overscrolling to make sure swipe-navigation
  // works with the APZ aware event listener.
  await panLeftToRight(tab.linkedBrowser, 100, 100, 1);

  let startLoadingPromise = BrowserTestUtils.browserStarted(
    tab.linkedBrowser,
    "about:about"
  );
  let stoppedLoadingPromise = BrowserTestUtils.browserStopped(
    tab.linkedBrowser,
    "about:about"
  );

  await Promise.all([startLoadingPromise, stoppedLoadingPromise]);

  ok(gBrowser.webNavigation.canGoForward);

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});

// NOTE: This test listens wheel events so that it causes an overscroll issue
// (bug 1800022). To avoid the bug, we need to run this test case at the end
// of this file.
add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.gesture.swipe.left", "Browser:BackOrBackDuplicate"],
      ["browser.gesture.swipe.right", "Browser:ForwardOrForwardDuplicate"],
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

  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, secondPage);
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
  is(wheelEventCount, 2, "Received 2 wheel events");

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
  await SpecialPowers.popPrefEnv();
});
