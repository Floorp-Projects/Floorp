/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that if the user mousedown's on a Picture-in-Picture toggle,
 * but then mouseup's on something completely different, that we still
 * open a Picture-in-Picture window, and that the mouse button events are
 * all suppressed. Also ensures that a subsequent click on the document
 * body results in all mouse button events firing normally.
 */
add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await SimpleTest.promiseFocus(browser);
      await ensureVideosReady(browser);
      let videoID = "no-controls";

      await prepareForToggleClick(browser, videoID);

      // Hover the mouse over the video to reveal the toggle, which is necessary
      // if we want to click on the toggle.
      await BrowserTestUtils.synthesizeMouseAtCenter(
        `#${videoID}`,
        {
          type: "mousemove",
        },
        browser
      );
      await BrowserTestUtils.synthesizeMouseAtCenter(
        `#${videoID}`,
        {
          type: "mouseover",
        },
        browser
      );

      info("Waiting for toggle to become visible");
      await toggleOpacityReachesThreshold(
        browser,
        videoID,
        HOVER_VIDEO_OPACITY
      );

      let toggleClientRect = await getToggleClientRect(browser, videoID);

      // The toggle center, because of how it slides out, is actually outside
      // of the bounds of a click event. For now, we move the mouse in by a
      // hard-coded 2 pixels along the x and y axis to achieve the hover.
      let toggleLeft = toggleClientRect.left + 2;
      let toggleTop = toggleClientRect.top + 2;

      info(
        "Clicking on toggle, and expecting a Picture-in-Picture window to open"
      );
      // We need to wait for the window to have completed loading before we
      // can close it as the document's type required by closeWindow may not
      // be available.
      let domWindowOpened = BrowserTestUtils.domWindowOpenedAndLoaded(null);

      await BrowserTestUtils.synthesizeMouseAtPoint(
        toggleLeft,
        toggleTop,
        {
          type: "mousedown",
        },
        browser
      );

      await BrowserTestUtils.synthesizeMouseAtPoint(
        1,
        1,
        {
          type: "mouseup",
        },
        browser
      );

      let win = await domWindowOpened;
      ok(win, "A Picture-in-Picture window opened.");
      await BrowserTestUtils.closeWindow(win);
      await assertSawMouseEvents(browser, false);

      await BrowserTestUtils.synthesizeMouseAtPoint(1, 1, {}, browser);
      await assertSawMouseEvents(browser, true);
    }
  );
});
