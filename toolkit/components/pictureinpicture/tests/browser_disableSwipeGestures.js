/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js",
  this
);

add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let pipWin = await triggerPictureInPicture(browser, "with-controls");

      let receivedSwipeGestureEvents = false;
      pipWin.addEventListener("MozSwipeGestureMayStart", () => {
        receivedSwipeGestureEvents = true;
      });

      const wheelEventPromise = BrowserTestUtils.waitForEvent(pipWin, "wheel");

      // Try swiping left to right.
      await panLeftToRightBegin(pipWin, 100, 100, 100);
      await panLeftToRightEnd(pipWin, 100, 100, 100);

      // Wait a wheel event and a couple of frames to give a chance to receive
      // the MozSwipeGestureMayStart event.
      await wheelEventPromise;
      await new Promise(resolve => requestAnimationFrame(resolve));
      await new Promise(resolve => requestAnimationFrame(resolve));

      Assert.ok(
        !receivedSwipeGestureEvents,
        "No swipe gesture events observed"
      );

      let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      pipWin.close();
      await pipClosed;
    }
  );
});
