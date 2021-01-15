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
      let controls = pipWin.document.getElementById("controls");

      await promiseFullscreenEntered(pipWin, async () => {
        EventUtils.sendMouseEvent(
          {
            type: "dblclick",
          },
          controls,
          pipWin
        );
      });

      Assert.equal(
        pipWin.document.fullscreenElement,
        pipWin.document.body,
        "Double-click caused us to enter fullscreen."
      );

      // First, we'll test exiting fullscreen by double-clicking again
      // on the document body.

      await promiseFullscreenExited(pipWin, async () => {
        EventUtils.sendMouseEvent(
          {
            type: "dblclick",
          },
          controls,
          pipWin
        );
      });

      Assert.ok(
        !pipWin.document.fullscreenElement,
        "Double-click caused us to exit fullscreen."
      );

      // Now we double-click to re-enter fullscreen.

      await promiseFullscreenEntered(pipWin, async () => {
        EventUtils.sendMouseEvent(
          {
            type: "dblclick",
          },
          controls,
          pipWin
        );
      });

      Assert.equal(
        pipWin.document.fullscreenElement,
        pipWin.document.body,
        "Double-click caused us to re-enter fullscreen."
      );

      // Finally, we check that hitting Escape lets the user leave
      // fullscreen.

      await promiseFullscreenExited(pipWin, async () => {
        EventUtils.synthesizeKey("KEY_Escape", {}, pipWin);
      });

      Assert.ok(
        !pipWin.document.fullscreenElement,
        "Pressing Escape caused us to exit fullscreen."
      );

      let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      pipWin.close();
      await pipClosed;
    }
  );
});
