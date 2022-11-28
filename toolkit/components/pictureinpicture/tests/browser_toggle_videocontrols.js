/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the Picture-in-Picture toggle is hidden when opening the closed captions menu
 * and is visible when closing the closed captions menu.
 */
add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "media.videocontrols.picture-in-picture.display-text-tracks.enabled",
        true,
      ],
      ["media.videocontrols.picture-in-picture.video-toggle.enabled", true],
    ],
  });
  let videoID = "with-controls";

  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE_WITH_WEBVTT,
      gBrowser,
    },
    async browser => {
      await prepareVideosAndWebVTTTracks(browser, videoID, -1);
      await prepareForToggleClick(browser, videoID);

      let args = {
        videoID,
        DEFAULT_TOGGLE_STYLES,
      };

      await SpecialPowers.spawn(browser, [args], async args => {
        let { videoID, DEFAULT_TOGGLE_STYLES } = args;
        let video = this.content.document.getElementById(videoID);
        let shadowRoot = video.openOrClosedShadowRoot;
        let closedCaptionButton = shadowRoot.querySelector(
          "#closedCaptionButton"
        );
        let toggle = shadowRoot.querySelector(
          `#${DEFAULT_TOGGLE_STYLES.rootID}`
        );
        let textTrackListContainer = shadowRoot.querySelector(
          "#textTrackListContainer"
        );

        Assert.ok(!toggle.hidden, "Toggle should be visible");
        Assert.ok(
          textTrackListContainer.hidden,
          "textTrackListContainer should be hidden"
        );

        info("Opening text track list from cc button");
        closedCaptionButton.click();

        Assert.ok(toggle.hidden, "Toggle should be hidden");
        Assert.ok(
          !textTrackListContainer.hidden,
          "textTrackListContainer should be visible"
        );

        info("Clicking the cc button again to close it");
        closedCaptionButton.click();

        Assert.ok(!toggle.hidden, "Toggle should be visible again");
        Assert.ok(
          textTrackListContainer.hidden,
          "textTrackListContainer should be hidden again"
        );
      });
    }
  );
});
