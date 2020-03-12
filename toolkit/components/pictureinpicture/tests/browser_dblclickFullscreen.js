/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that double-clicking on the Picture-in-Picture player window
 * causes it to fullscreen, and that pressing Escape allows us to exit
 * fullscreen.
 */
add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      let pipWin = await triggerPictureInPicture(browser, "no-controls");

      let entered = BrowserTestUtils.waitForEvent(
        pipWin,
        "MozDOMFullscreen:Entered"
      );

      let controls = pipWin.document.getElementById("controls");

      EventUtils.sendMouseEvent(
        {
          type: "dblclick",
        },
        controls,
        pipWin
      );

      await entered;

      Assert.equal(
        pipWin.document.fullscreenElement,
        pipWin.document.body,
        "Double-click caused us to enter fullscreen."
      );

      await BrowserTestUtils.waitForCondition(() => {
        return !TelemetryStopwatch.running("FULLSCREEN_CHANGE_MS");
      });

      // First, we'll test exiting fullscreen by double-clicking again
      // on the document body.

      let exited = BrowserTestUtils.waitForEvent(
        pipWin,
        "MozDOMFullscreen:Exited"
      );

      EventUtils.sendMouseEvent(
        {
          type: "dblclick",
        },
        controls,
        pipWin
      );

      await exited;

      Assert.ok(
        !pipWin.document.fullscreenElement,
        "Double-click caused us to exit fullscreen."
      );

      await BrowserTestUtils.waitForCondition(() => {
        return !TelemetryStopwatch.running("FULLSCREEN_CHANGE_MS");
      });

      // Now we double-click to re-enter fullscreen.

      entered = BrowserTestUtils.waitForEvent(
        pipWin,
        "MozDOMFullscreen:Entered"
      );

      EventUtils.sendMouseEvent(
        {
          type: "dblclick",
        },
        controls,
        pipWin
      );

      await entered;

      Assert.equal(
        pipWin.document.fullscreenElement,
        pipWin.document.body,
        "Double-click caused us to re-enter fullscreen."
      );

      await BrowserTestUtils.waitForCondition(() => {
        return !TelemetryStopwatch.running("FULLSCREEN_CHANGE_MS");
      });

      // Finally, we check that hitting Escape lets the user leave
      // fullscreen.

      exited = BrowserTestUtils.waitForEvent(pipWin, "MozDOMFullscreen:Exited");

      EventUtils.synthesizeKey("KEY_Escape", {}, pipWin);

      await exited;

      Assert.ok(
        !pipWin.document.fullscreenElement,
        "Pressing Escape caused us to exit fullscreen."
      );

      await BrowserTestUtils.waitForCondition(() => {
        return !TelemetryStopwatch.running("FULLSCREEN_CHANGE_MS");
      });

      let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      pipWin.close();
      await pipClosed;
    }
  );
});
