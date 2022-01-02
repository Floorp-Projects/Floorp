/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that if a <video> element is being displayed in a
 * Picture-in-Picture window, that the window closes if that
 * original <video> is ever removed from the DOM.
 */
add_task(async () => {
  for (let videoID of ["with-controls", "no-controls"]) {
    info(`Testing ${videoID} case.`);

    await BrowserTestUtils.withNewTab(
      {
        url: TEST_PAGE,
        gBrowser,
      },
      async browser => {
        let pipWin = await triggerPictureInPicture(browser, videoID);
        Assert.ok(pipWin, "Got PiP window.");

        // First, let's make sure that removing the _other_ video doesn't cause
        // the special event to fire, nor the PiP window to close.
        await SpecialPowers.spawn(browser, [videoID], async videoID => {
          let doc = content.document;
          let otherVideo = doc.querySelector(`video:not([id="${videoID}"])`);
          let eventFired = false;

          let listener = e => {
            eventFired = true;
          };

          docShell.chromeEventHandler.addEventListener(
            "MozStopPictureInPicture",
            listener,
            {
              capture: true,
            }
          );
          otherVideo.remove();
          Assert.ok(
            !eventFired,
            "Should not have seen MozStopPictureInPicture for other video"
          );
          docShell.chromeEventHandler.removeEventListener(
            "MozStopPictureInPicture",
            listener,
            {
              capture: true,
            }
          );
        });

        Assert.ok(!pipWin.closed, "PiP window should still be open.");

        await SpecialPowers.spawn(browser, [videoID], async videoID => {
          let doc = content.document;
          let video = doc.querySelector(`#${videoID}`);

          let promise = ContentTaskUtils.waitForEvent(
            docShell.chromeEventHandler,
            "MozStopPictureInPicture",
            { capture: true }
          );
          video.remove();
          await promise;
        });

        try {
          await BrowserTestUtils.waitForCondition(
            () => pipWin.closed,
            "Player window closed."
          );
        } finally {
          if (!pipWin.closed) {
            await BrowserTestUtils.closeWindow(pipWin);
          }
        }
      }
    );
  }
});
