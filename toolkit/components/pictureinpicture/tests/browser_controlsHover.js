/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests functionality for the hover states of the various controls for the Picture-in-Picture
 * video window.
 */
add_task(async () => {
  let videoID = "with-controls";

  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      let waitForVideoEvent = eventType => {
        return BrowserTestUtils.waitForContentEvent(browser, eventType, true);
      };

      await ensureVideosReady(browser);
      await SpecialPowers.spawn(browser, [videoID], async videoID => {
        await content.document.getElementById(videoID).play();
      });

      // Open the video in PiP
      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      const l10n = new Localization(
        ["toolkit/pictureinpicture/pictureinpicture.ftl"],
        true
      );

      let [
        close,
        play,
        unmute,
        unpip,
        subtitles,
        pause,
        mute,
        fullscreenEnter,
        fullscreenExit,
      ] = l10n.formatMessagesSync([
        {
          id: "pictureinpicture-close-btn",
          args: {
            shortcut: ShortcutUtils.prettifyShortcut(
              pipWin.document.getElementById("closeShortcut")
            ),
          },
        },
        { id: "pictureinpicture-play-btn" },
        {
          id: "pictureinpicture-unmute-btn",
          args: {
            shortcut: ShortcutUtils.prettifyShortcut(
              pipWin.document.getElementById("unMuteShortcut")
            ),
          },
        },
        { id: "pictureinpicture-unpip-btn" },
        { id: "pictureinpicture-subtitles-btn" },
        { id: "pictureinpicture-pause-btn" },
        {
          id: "pictureinpicture-mute-btn",
          args: {
            shortcut: ShortcutUtils.prettifyShortcut(
              pipWin.document.getElementById("muteShortcut")
            ),
          },
        },
        {
          id: "pictureinpicture-fullscreen-btn2",
          args: {
            shortcut: ShortcutUtils.prettifyShortcut(
              pipWin.document.getElementById("fullscreenToggleShortcut")
            ),
          },
        },
        {
          id: "pictureinpicture-exit-fullscreen-btn2",
          args: {
            shortcut: ShortcutUtils.prettifyShortcut(
              pipWin.document.getElementById("fullscreenToggleShortcut")
            ),
          },
        },
      ]);

      let closeButton = pipWin.document.getElementById("close");
      let playPauseButton = pipWin.document.getElementById("playpause");
      let unpipButton = pipWin.document.getElementById("unpip");
      let muteUnmuteButton = pipWin.document.getElementById("audio");
      let subtitlesButton = pipWin.document.getElementById("closed-caption");
      let fullscreenButton = pipWin.document.getElementById("fullscreen");

      // checks hover title for close button
      await pipWin.document.l10n.translateFragment(closeButton);
      Assert.equal(
        close.attributes[1].value,
        closeButton.getAttribute("tooltip"),
        "The close button title matches Fluent string"
      );

      // checks hover title for play button
      await pipWin.document.l10n.translateFragment(playPauseButton);
      Assert.equal(
        pause.attributes[1].value,
        playPauseButton.getAttribute("tooltip"),
        "The play button title matches Fluent string"
      );

      // checks hover title for unpip button
      await pipWin.document.l10n.translateFragment(unpipButton);
      Assert.equal(
        unpip.attributes[1].value,
        unpipButton.getAttribute("tooltip"),
        "The unpip button title matches Fluent string"
      );

      // checks hover title for subtitles button
      await pipWin.document.l10n.translateFragment(subtitlesButton);
      Assert.equal(
        subtitles.attributes[1].value,
        subtitlesButton.getAttribute("tooltip"),
        "The subtitles button title matches Fluent string"
      );

      // checks hover title for unmute button
      await pipWin.document.l10n.translateFragment(muteUnmuteButton);
      Assert.equal(
        mute.attributes[1].value,
        muteUnmuteButton.getAttribute("tooltip"),
        "The Unmute button title matches Fluent string"
      );

      // Pause the video
      let pausedPromise = waitForVideoEvent("pause");
      EventUtils.synthesizeMouseAtCenter(playPauseButton, {}, pipWin);
      await pausedPromise;
      ok(await isVideoPaused(browser, videoID), "The video is paused.");

      // checks hover title for pause button
      await pipWin.document.l10n.translateFragment(playPauseButton);
      Assert.equal(
        play.attributes[1].value,
        playPauseButton.getAttribute("tooltip"),
        "The pause button title matches Fluent string"
      );

      // Mute the video
      let mutedPromise = waitForVideoEvent("volumechange");
      EventUtils.synthesizeMouseAtCenter(muteUnmuteButton, {}, pipWin);
      await mutedPromise;
      ok(await isVideoMuted(browser, videoID), "The audio is muted.");

      // checks hover title for mute button
      await pipWin.document.l10n.translateFragment(muteUnmuteButton);
      Assert.equal(
        unmute.attributes[1].value,
        muteUnmuteButton.getAttribute("tooltip"),
        "The mute button title matches Fluent string"
      );

      // checks hover title for enter fullscreen button
      await pipWin.document.l10n.translateFragment(fullscreenButton);
      Assert.equal(
        fullscreenEnter.attributes[1].value,
        fullscreenButton.getAttribute("tooltip"),
        "The enter fullscreen button title matches Fluent string"
      );

      // enable fullscreen
      await promiseFullscreenEntered(pipWin, async () => {
        EventUtils.synthesizeMouseAtCenter(fullscreenButton, {}, pipWin);
      });

      // checks hover title for exit fullscreen button
      await pipWin.document.l10n.translateFragment(fullscreenButton);
      Assert.equal(
        fullscreenExit.attributes[1].value,
        fullscreenButton.getAttribute("tooltip"),
        "The exit fullscreen button title matches Fluent string"
      );
    }
  );
});
