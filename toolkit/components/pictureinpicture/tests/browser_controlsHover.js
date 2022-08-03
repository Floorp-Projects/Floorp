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
      ] = l10n.formatMessagesSync([
        {
          id: "pictureinpicture-close-cmd",
          args: {
            shortcut: ShortcutUtils.prettifyShortcut(
              pipWin.document.getElementById("closeShortcut")
            ),
          },
        },
        { id: "pictureinpicture-play-cmd" },
        {
          id: "pictureinpicture-unmute-cmd",
          args: {
            shortcut: ShortcutUtils.prettifyShortcut(
              pipWin.document.getElementById("unMuteShortcut")
            ),
          },
        },
        { id: "pictureinpicture-unpip-cmd" },
        { id: "pictureinpicture-subtitles-cmd" },
        { id: "pictureinpicture-pause-cmd" },
        {
          id: "pictureinpicture-mute-cmd",
          args: {
            shortcut: ShortcutUtils.prettifyShortcut(
              pipWin.document.getElementById("muteShortcut")
            ),
          },
        },
      ]);

      let closeButton = pipWin.document.getElementById("close");
      let playPauseButton = pipWin.document.getElementById("playpause");
      let unpipButton = pipWin.document.getElementById("unpip");
      let muteUnmuteButton = pipWin.document.getElementById("audio");
      let subtitlesButton = pipWin.document.getElementById("closed-caption");

      // checks hover title for close button
      await pipWin.document.l10n.translateFragment(closeButton);
      Assert.equal(
        close.attributes[1].value,
        closeButton.title,
        "The close button title matches Fluent string"
      );

      // checks hover title for play button
      await pipWin.document.l10n.translateFragment(playPauseButton);
      Assert.equal(
        pause.attributes[1].value,
        playPauseButton.title,
        "The play button title matches Fluent string"
      );

      // checks hover title for unpip button
      await pipWin.document.l10n.translateFragment(unpipButton);
      Assert.equal(
        unpip.attributes[1].value,
        unpipButton.title,
        "The unpip button title matches Fluent string"
      );

      // checks hover title for subtitles button
      await pipWin.document.l10n.translateFragment(subtitlesButton);
      Assert.equal(
        subtitles.attributes[1].value,
        subtitlesButton.title,
        "The subtitles button title matches Fluent string"
      );

      // checks hover title for unmute button
      await pipWin.document.l10n.translateFragment(muteUnmuteButton);
      Assert.equal(
        mute.attributes[1].value,
        muteUnmuteButton.title,
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
        playPauseButton.title,
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
        muteUnmuteButton.title,
        "The mute button title matches Fluent string"
      );
    }
  );
});
