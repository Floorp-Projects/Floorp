/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This function tests changing the src of a Picture-in-Picture player while
 * the player is fullscreened and then ensuring the that video stays
 * fullscreened after the src change and that the player will resize to the new
 * video size.
 */
add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },

    async browser => {
      async function switchVideoSource(src) {
        await ContentTask.spawn(browser, { src }, async ({ src }) => {
          let doc = content.document;
          let video = doc.getElementById("no-controls");
          video.src = src;
        });
      }

      const NEW_VIDEO_ASPECT_RATIO = 1.334;

      function checkIfEqual(val1, val2, str) {
        let equal = Math.abs(val1 - val2);
        if (equal <= 1) {
          is(equal <= 1, true, str);
        } else {
          is(val1, val2, str);
        }
      }

      let pipWin = await triggerPictureInPicture(browser, "no-controls");

      let resizeCount = 0;
      pipWin.addEventListener("resize", e => {
        resizeCount++;
      });

      let left = 100;
      let top = 100;
      pipWin.moveTo(left, top);
      let width = pipWin.innerWidth / 2;
      let height = pipWin.innerHeight / 2;
      pipWin.resizeTo(width, height);

      await BrowserTestUtils.waitForCondition(() => resizeCount == 1);

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

      await BrowserTestUtils.waitForCondition(() => resizeCount == 2);

      await switchVideoSource("test-video.mp4");

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

      // Would be 4 but two resizes that happen very close together are
      // optimized by "coalescing" the two resizes into one resize
      await BrowserTestUtils.waitForCondition(() => resizeCount == 3);

      checkIfEqual(pipWin.screenX, left, "PiP in same X location");
      checkIfEqual(pipWin.screenY, top, "PiP in same Y location");
      checkIfEqual(pipWin.innerHeight, height, "PiP has same height");
      checkIfEqual(pipWin.innerWidth, width, "PiP has same width");

      await ensureMessageAndClosePiP(browser, "no-controls", pipWin, true);

      pipWin = await triggerPictureInPicture(browser, "no-controls");

      resizeCount = 0;
      pipWin.addEventListener("resize", e => {
        resizeCount++;
      });

      controls = pipWin.document.getElementById("controls");

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

      await BrowserTestUtils.waitForCondition(() => resizeCount == 1);

      await switchVideoSource("test-video-long.mp4");

      await promiseFullscreenExited(pipWin, async () => {
        EventUtils.synthesizeKey("KEY_Escape", {}, pipWin);
      });

      Assert.ok(
        !pipWin.document.fullscreenElement,
        "Double-click caused us to exit fullscreen."
      );

      // Would be 3 but two resizes that happen very close together are
      // optimized by "coalescing" the two resizes into one resize
      await BrowserTestUtils.waitForCondition(() => resizeCount == 2);

      checkIfEqual(pipWin.screenX, left, "PiP in same X location");
      checkIfEqual(pipWin.screenY, top, "PiP in same Y location");
      checkIfEqual(pipWin.innerHeight, height, "PiP has same height");
      checkIfEqual(
        pipWin.innerWidth,
        height * NEW_VIDEO_ASPECT_RATIO,
        "PiP has same width"
      );

      await ensureMessageAndClosePiP(browser, "no-controls", pipWin, true);

      pipWin = await triggerPictureInPicture(browser, "no-controls");

      resizeCount = 0;
      pipWin.addEventListener("resize", e => {
        resizeCount++;
      });

      controls = pipWin.document.getElementById("controls");

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

      await BrowserTestUtils.waitForCondition(() => resizeCount == 1);

      await promiseFullscreenExited(pipWin, async () => {
        EventUtils.synthesizeKey("KEY_Escape", {}, pipWin);
      });

      Assert.ok(
        !pipWin.document.fullscreenElement,
        "Double-click caused us to exit fullscreen."
      );

      await BrowserTestUtils.waitForCondition(() => resizeCount == 2);

      checkIfEqual(pipWin.screenX, left, "PiP in same X location");
      checkIfEqual(pipWin.screenY, top, "PiP in same Y location");
      checkIfEqual(pipWin.innerHeight, height, "PiP has same height");
      checkIfEqual(
        pipWin.innerWidth,
        height * NEW_VIDEO_ASPECT_RATIO,
        "PiP has same width"
      );

      await ensureMessageAndClosePiP(browser, "no-controls", pipWin, true);
    }
  );
});
