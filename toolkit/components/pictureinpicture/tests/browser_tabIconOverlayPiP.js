/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * The goal of this test is check the that "tab-icon-overlay" image is
 * showing when the tab is using PiP.
 *
 * The browser will create a tab and open a video using PiP
 * then the tests check that the tab icon overlay image is showing*
 *
 *
 */
add_task(async () => {
  let videoID = "with-controls";
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE_WITH_SOUND,
      gBrowser,
    },
    async browser => {
      await ensureVideosReady(browser);

      let audioPromise = BrowserTestUtils.waitForEvent(
        browser,
        "DOMAudioPlaybackStarted"
      );

      await SpecialPowers.spawn(browser, [videoID], async videoID => {
        await content.document.getElementById(videoID).play();
      });

      // Check that video is playing
      ok(!(await isVideoPaused(browser, videoID)), "The video is not paused.");
      await audioPromise;

      // Need tab to access the tab-icon-overlay element
      let tab = gBrowser.getTabForBrowser(browser);

      // Use tab to get the tab-icon-overlay element
      let tabIconOverlay = tab.getElementsByClassName("tab-icon-overlay")[0];

      // Not in PiP yet so the tab-icon-overlay does not have "pictureinpicture" attribute
      ok(!tabIconOverlay.hasAttribute("pictureinpicture"), "Not using PiP");

      // Sound is playing so tab should have "soundplaying" attribute
      ok(tabIconOverlay.hasAttribute("soundplaying"), "Sound is playing");

      // Start the PiP
      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      // Check that video is still playing
      ok(!(await isVideoPaused(browser, videoID)), "The video is not paused.");

      // Video is still playing so the tab-icon-overlay should have "soundplaying" as an attribute
      ok(
        tabIconOverlay.hasAttribute("soundplaying"),
        "Tab knows sound is playing"
      );

      // Now in PiP. "pictureinpicture" is an attribute
      ok(
        tabIconOverlay.hasAttribute("pictureinpicture"),
        "Tab knows were using PiP"
      );

      // We know the tab has sound playing and it is using PiP so we can check the
      // tab-icon-overlay image is showing
      let style = window.getComputedStyle(tabIconOverlay);
      Assert.equal(
        style.listStyleImage,
        'url("chrome://browser/skin/tabbrowser/tab-audio-playing-small.svg")',
        "Got the tab-icon-overlay image"
      );

      // Check tab is not muted
      ok(!tabIconOverlay.hasAttribute("muted"), "Tab is not muted");

      // Click on tab icon overlay to mute tab and check it is muted
      tabIconOverlay.click();
      ok(tabIconOverlay.hasAttribute("muted"), "Tab is muted");

      // Click on tab icon overlay to unmute tab and check it is not muted
      tabIconOverlay.click();
      ok(!tabIconOverlay.hasAttribute("muted"), "Tab is not muted");
    }
  );
});
