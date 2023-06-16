/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const PIP_SHORTCUT_OPEN_EVENTS = [
  {
    category: "pictureinpicture",
    method: "opened_method",
    object: "shortcut",
  },
];

const PIP_SHORTCUT_CLOSE_EVENTS = [
  {
    category: "pictureinpicture",
    method: "closed_method",
    object: "shortcut",
  },
];

/**
 * Tests that if the user keys in the keyboard shortcut for
 * Picture-in-Picture, then the first video on the currently
 * focused page will be opened in the player window.
 */
add_task(async function test_pip_keyboard_shortcut() {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      Services.telemetry.clearEvents();
      await ensureVideosReady(browser);

      // In test-page.html, the "with-controls" video is the first one that
      // appears in the DOM, so this is what we expect to open via the keyboard
      // shortcut.
      const VIDEO_ID = "with-controls";

      let domWindowOpened = BrowserTestUtils.domWindowOpenedAndLoaded(null);
      let videoReady = SpecialPowers.spawn(
        browser,
        [VIDEO_ID],
        async videoID => {
          let video = content.document.getElementById(videoID);
          await ContentTaskUtils.waitForCondition(() => {
            return video.isCloningElementVisually;
          }, "Video is being cloned visually.");
        }
      );

      if (AppConstants.platform == "macosx") {
        EventUtils.synthesizeKey("]", {
          accelKey: true,
          shiftKey: true,
          altKey: true,
        });
      } else {
        EventUtils.synthesizeKey("]", { accelKey: true, shiftKey: true });
      }

      let pipWin = await domWindowOpened;
      await videoReady;

      ok(pipWin, "Got Picture-in-Picture window.");

      await ensureMessageAndClosePiP(browser, VIDEO_ID, pipWin, false);

      let openFilter = {
        category: "pictureinpicture",
        method: "opened_method",
        object: "shortcut",
      };
      await waitForTelemeryEvents(
        openFilter,
        PIP_SHORTCUT_OPEN_EVENTS.length,
        "content"
      );
      TelemetryTestUtils.assertEvents(PIP_SHORTCUT_OPEN_EVENTS, openFilter, {
        clear: true,
        process: "content",
      });

      // Reopen PiP Window
      pipWin = await triggerPictureInPicture(browser, VIDEO_ID);
      await videoReady;

      ok(pipWin, "Got Picture-in-Picture window.");

      if (AppConstants.platform == "macosx") {
        EventUtils.synthesizeKey(
          "]",
          {
            accelKey: true,
            shiftKey: true,
            altKey: true,
          },
          pipWin
        );
      } else {
        EventUtils.synthesizeKey(
          "]",
          { accelKey: true, shiftKey: true },
          pipWin
        );
      }

      await BrowserTestUtils.windowClosed(pipWin);

      ok(pipWin.closed, "Picture-in-Picture window closed.");

      let closeFilter = {
        category: "pictureinpicture",
        method: "closed_method",
        object: "shortcut",
      };
      await waitForTelemeryEvents(
        closeFilter,
        PIP_SHORTCUT_CLOSE_EVENTS.length,
        "parent"
      );
      TelemetryTestUtils.assertEvents(PIP_SHORTCUT_CLOSE_EVENTS, closeFilter, {
        clear: true,
        process: "parent",
      });
    }
  );
});
