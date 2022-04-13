/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the pip window closes when the pagehide page lifecycle event
 * is not detected and if a video is not loaded with a src.
 */
add_task(async function test_close_empty_pip_window() {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      let videoID = "with-controls";

      await ensureVideosReady(browser);

      let emptied = SpecialPowers.spawn(browser, [{ videoID }], async args => {
        let video = content.document.getElementById(args.videoID);
        info("Waiting for emptied event to be called");
        await ContentTaskUtils.waitForEvent(video, "emptied");
      });

      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      await SpecialPowers.spawn(browser, [{ videoID }], async args => {
        let video = content.document.getElementById(args.videoID);
        video.removeAttribute("src");
        video.load();
      });
      await emptied;
      await pipClosed;
    }
  );
});

/**
 * Tests that the pip window closes when navigating to another page
 * via the pagehide page lifecycle event.
 */
add_task(async function test_close_pagehide() {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      let videoID = "with-controls";

      await ensureVideosReady(browser);
      await SpecialPowers.spawn(browser, [{ videoID }], async args => {
        let video = content.document.getElementById(args.videoID);
        video.onemptied = () => {
          // Since we handle pagehide first, handle emptied should not be invoked
          ok(false, "emptied not expected to be called");
        };
      });

      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      await SpecialPowers.spawn(browser, [{ videoID }], async args => {
        content.location.href = "otherpage.html";
      });

      await pipClosed;
    }
  );
});

/**
 * Tests that the pip window remains open if the pagehide page lifecycle
 * event is not detected and if the video is still loaded with a src.
 */
add_task(async function test_open_pip_window_history_nav() {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      let videoID = "with-controls";

      await ensureVideosReady(browser);

      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      await SpecialPowers.spawn(browser, [{ videoID }], async args => {
        let popStatePromise = ContentTaskUtils.waitForEvent(
          content,
          "popstate"
        );
        content.history.pushState({}, "new page", "test-page-with-sound.html");
        content.history.back();
        await popStatePromise;
      });

      ok(!pipWin.closed, "pip windows should still be open");

      let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      let closeButton = pipWin.document.getElementById("close");
      EventUtils.synthesizeMouseAtCenter(closeButton, {}, pipWin);
      await pipClosed;
    }
  );
});
