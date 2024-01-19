/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that pressing the Escape key will close the subtitles settings panel and
 * not remove focus if activated via the mouse.
 */
add_task(async function test_closePanelESCMouseFocus() {
  clearSavedPosition();

  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE_WITH_WEBVTT,
      gBrowser,
    },
    async browser => {
      await SpecialPowers.pushPrefEnv({
        set: [
          [
            "media.videocontrols.picture-in-picture.display-text-tracks.enabled",
            true,
          ],
          [
            "media.videocontrols.picture-in-picture.display-text-tracks.size",
            "medium",
          ],
        ],
      });

      let videoID = "with-controls";

      await ensureVideosReady(browser);
      await prepareVideosAndWebVTTTracks(browser, videoID);
      await SpecialPowers.spawn(browser, [videoID], async videoID => {
        await content.document.getElementById(videoID).play();
      });

      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      // Resize PiP window so that subtitles button is visible
      let resizePromise = BrowserTestUtils.waitForEvent(pipWin, "resize");
      pipWin.resizeTo(640, 360);
      await resizePromise;

      let subtitlesButton = pipWin.document.getElementById("closed-caption");
      Assert.ok(subtitlesButton, "Subtitles button found");

      let subtitlesPanel = pipWin.document.getElementById("settings");
      let panelVisiblePromise = BrowserTestUtils.waitForCondition(
        () => BrowserTestUtils.isVisible(subtitlesPanel),
        "Wait for panel to be visible"
      );

      EventUtils.synthesizeMouseAtCenter(subtitlesButton, {}, pipWin);

      await panelVisiblePromise;

      let audioButton = pipWin.document.getElementById("audio");
      audioButton.focus();

      let panelHiddenPromise = BrowserTestUtils.waitForCondition(
        () => BrowserTestUtils.isHidden(subtitlesPanel),
        "Wait for panel to be hidden"
      );

      EventUtils.synthesizeKey("KEY_Escape", {}, pipWin);

      info("Make sure subtitles settings panel closes after pressing ESC");
      await panelHiddenPromise;

      Assert.notEqual(
        pipWin.document.activeElement,
        subtitlesButton,
        "Subtitles button does not have focus after closing panel"
      );
      Assert.ok(pipWin, "PiP window is still open");

      clearSavedPosition();
    }
  );
});

/**
 * Tests that pressing the Escape key will close the subtitles settings panel and
 * refocus on the subtitles button if activated via the keyboard.
 */
add_task(async function test_closePanelESCKeyboardFocus() {
  clearSavedPosition();

  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE_WITH_WEBVTT,
      gBrowser,
    },
    async browser => {
      await SpecialPowers.pushPrefEnv({
        set: [
          [
            "media.videocontrols.picture-in-picture.display-text-tracks.enabled",
            true,
          ],
        ],
      });

      let videoID = "with-controls";

      await ensureVideosReady(browser);
      await prepareVideosAndWebVTTTracks(browser, videoID);
      await SpecialPowers.spawn(browser, [videoID], async videoID => {
        await content.document.getElementById(videoID).play();
      });

      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      // Resize PiP window so that subtitles button is visible
      let resizePromise = BrowserTestUtils.waitForEvent(pipWin, "resize");
      pipWin.resizeTo(640, 360);
      await resizePromise;

      let subtitlesButton = pipWin.document.getElementById("closed-caption");
      Assert.ok(subtitlesButton, "Subtitles button found");

      let subtitlesPanel = pipWin.document.getElementById("settings");
      let subtitlesToggle = pipWin.document.getElementById("subtitles-toggle");
      let panelVisiblePromise = BrowserTestUtils.waitForCondition(
        () => BrowserTestUtils.isVisible(subtitlesPanel),
        "Wait for panel to be visible"
      );

      subtitlesButton.focus();
      EventUtils.synthesizeKey(" ", {}, pipWin);

      await panelVisiblePromise;

      Assert.equal(
        pipWin.document.activeElement,
        subtitlesToggle,
        "Subtitles switch toggle should have focus after opening panel"
      );

      let panelHiddenPromise = BrowserTestUtils.waitForCondition(
        () => BrowserTestUtils.isHidden(subtitlesPanel),
        "Wait for panel to be hidden"
      );

      EventUtils.synthesizeKey("KEY_Escape", {}, pipWin);

      info("Make sure subtitles settings panel closes after pressing ESC");
      await panelHiddenPromise;

      Assert.equal(
        pipWin.document.activeElement,
        subtitlesButton,
        "Subtitles button has focus after closing panel"
      );
      Assert.ok(pipWin, "PiP window is still open");

      clearSavedPosition();
    }
  );
});

/**
 * Tests keyboard navigation for the subtitles settings panel and that it closes after selecting
 * the subtitles button.
 */
add_task(async function test_panelKeyboardButtons() {
  clearSavedPosition();

  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE_WITH_WEBVTT,
      gBrowser,
    },
    async browser => {
      await SpecialPowers.pushPrefEnv({
        set: [
          [
            "media.videocontrols.picture-in-picture.display-text-tracks.enabled",
            true,
          ],
        ],
      });

      let videoID = "with-controls";

      await ensureVideosReady(browser);
      await prepareVideosAndWebVTTTracks(browser, videoID);
      await SpecialPowers.spawn(browser, [videoID], async videoID => {
        await content.document.getElementById(videoID).play();
        // Mute video
        content.document.getElementById(videoID).muted = true;
      });

      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      // Resize PiP window so that subtitles button is visible
      let resizePromise = BrowserTestUtils.waitForEvent(pipWin, "resize");
      pipWin.resizeTo(640, 360);
      await resizePromise;

      let subtitlesButton = pipWin.document.getElementById("closed-caption");
      Assert.ok(subtitlesButton, "Subtitles button found");

      let subtitlesPanel = pipWin.document.getElementById("settings");
      let subtitlesToggle = pipWin.document.getElementById("subtitles-toggle");
      let panelVisiblePromise = BrowserTestUtils.waitForCondition(
        () => BrowserTestUtils.isVisible(subtitlesPanel),
        "Wait for panel to be visible"
      );

      subtitlesButton.focus();
      EventUtils.synthesizeKey(" ", {}, pipWin);

      await panelVisiblePromise;

      Assert.equal(
        pipWin.document.activeElement,
        subtitlesToggle,
        "Subtitles switch toggle should have focus after opening panel"
      );

      let fontMediumRadio = pipWin.document.getElementById("medium");
      EventUtils.synthesizeKey("KEY_Tab", {}, pipWin);

      Assert.equal(
        pipWin.document.activeElement,
        fontMediumRadio,
        "Medium font size radio button should have focus"
      );

      let fontSmallRadio = pipWin.document.getElementById("small");
      EventUtils.synthesizeKey("KEY_ArrowUp", {}, pipWin);

      Assert.equal(
        pipWin.document.activeElement,
        fontSmallRadio,
        "Small font size radio button should have focus"
      );
      Assert.ok(isVideoMuted(browser, videoID), "Video should still be muted");
      Assert.equal(
        SpecialPowers.getCharPref(
          "media.videocontrols.picture-in-picture.display-text-tracks.size"
        ),
        "small",
        "Font size changed to small"
      );

      subtitlesButton.focus();

      let panelHiddenPromise = BrowserTestUtils.waitForCondition(
        () => BrowserTestUtils.isHidden(subtitlesPanel),
        "Wait for panel to be hidden"
      );

      EventUtils.synthesizeKey(" ", {}, pipWin);

      info(
        "Make sure subtitles settings panel closes after pressing the subtitles button"
      );
      await panelHiddenPromise;

      Assert.ok(pipWin, "PiP window is still open");

      clearSavedPosition();
    }
  );
});
