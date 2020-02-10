/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that closing with unpip leaves the video playing but the close button
 * will pause the video.
 */
add_task(async () => {
  for (let videoID of ["with-controls", "no-controls"]) {
    info(`Testing ${videoID} case.`);

    let playVideo = () => {
      return SpecialPowers.spawn(browser, [videoID], async videoID => {
        return content.document.getElementById(videoID).play();
      });
    };
    let isVideoPaused = () => {
      return SpecialPowers.spawn(browser, [videoID], async videoID => {
        return content.document.getElementById(videoID).paused;
      });
    };

    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);
    let browser = tab.linkedBrowser;
    await playVideo();

    // Try the unpip button.
    let pipWin = await triggerPictureInPicture(browser, videoID);
    ok(pipWin, "Got Picture-in-Picture window.");
    ok(!(await isVideoPaused()), "The video is not paused");

    let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
    let unpipButton = pipWin.document.getElementById("unpip");
    EventUtils.synthesizeMouseAtCenter(unpipButton, {}, pipWin);
    await pipClosed;
    ok(!(await isVideoPaused()), "The video is not paused");

    // Try the close button.
    pipWin = await triggerPictureInPicture(browser, videoID);
    ok(pipWin, "Got Picture-in-Picture window.");
    ok(!(await isVideoPaused()), "The video is not paused");

    pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
    let closeButton = pipWin.document.getElementById("close");
    EventUtils.synthesizeMouseAtCenter(closeButton, {}, pipWin);
    await pipClosed;
    ok(await isVideoPaused(), "The video is paused");

    BrowserTestUtils.removeTab(tab);
  }
});
