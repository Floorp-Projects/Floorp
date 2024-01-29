/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * tests that the ESC key would stop the player and close the PiP player floating window
 */
add_task(async () => {
  for (let videoID of ["with-controls", "no-controls"]) {
    info(`Testing ${videoID} case.`);

    let playVideo = () => {
      return SpecialPowers.spawn(browser, [videoID], async videoID => {
        return content.document.getElementById(videoID).play();
      });
    };

    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_PAGE);
    let browser = tab.linkedBrowser;

    await playVideo();

    let pipWin = await triggerPictureInPicture(browser, videoID);
    ok(pipWin, "The Picture-in-Picture window is not there.");
    ok(
      !(await isVideoPaused(browser, videoID)),
      "The video is paused, but should not."
    );
    ok(
      !pipWin.document.fullscreenElement,
      "PiP should not yet be in fullscreen."
    );

    let controls = pipWin.document.getElementById("controls");
    await promiseFullscreenEntered(pipWin, async () => {
      EventUtils.sendMouseEvent({ type: "dblclick" }, controls, pipWin);
    });

    Assert.equal(
      pipWin.document.fullscreenElement,
      pipWin.document.body,
      "Double-click should have caused to enter fullscreen."
    );

    await promiseFullscreenExited(pipWin, async () => {
      EventUtils.synthesizeKey("KEY_Escape", {}, pipWin);
    });

    ok(
      !pipWin.document.fullscreenElement,
      "ESC should have caused to leave fullscreen."
    );
    ok(
      !(await isVideoPaused(browser, videoID)),
      "The video is paused, but should not."
    );

    // Try to close the PiP window via the ESC button, since now it is not in fullscreen anymore.
    EventUtils.synthesizeKey("KEY_Escape", {}, pipWin);

    // then the PiP should have been closed
    ok(pipWin.closed, "Picture-in-Picture window is not closed, but should.");
    // and the video should not be playing anymore
    ok(
      await isVideoPaused(browser, videoID),
      "The video is not paused, but should."
    );

    await BrowserTestUtils.removeTab(tab);
  }
});
