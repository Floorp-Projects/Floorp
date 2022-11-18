/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the Picture-in-Picture does not start when clicking the toggle
 * over interfering video controls settings, such as the subtitles/closed captions menu.
 */
add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "media.videocontrols.picture-in-picture.display-text-tracks.enabled",
        true,
      ],
      ["media.videocontrols.picture-in-picture.video-toggle.enabled", true],
      ["media.videocontrols.picture-in-picture.video-toggle.position", "right"],
      [
        "media.videocontrols.picture-in-picture.video-toggle.visibility-threshold",
        "1.0",
      ],
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

      let toggleStyles = DEFAULT_TOGGLE_STYLES;
      let stage = "hoverVideo";
      let toggleStylesForStage = toggleStyles.stages[stage];
      let toggleClientRect = await getToggleClientRect(browser, videoID);

      let args = {
        videoID,
        toggleClientRect,
        toggleStylesForStage,
      };

      await SpecialPowers.spawn(browser, [args], async args => {
        // waitForToggleOpacity is based on toggleOpacityReachesThreshold.
        // Waits for toggle to reach target opacity.
        async function waitForToggleOpacity(shadowRoot, toggleStylesForStage) {
          for (let hiddenElement of toggleStylesForStage.hidden) {
            let el = shadowRoot.querySelector(hiddenElement);
            ok(
              ContentTaskUtils.is_hidden(el),
              `Expected ${hiddenElement} to be hidden.`
            );
          }

          for (let opacityElement in toggleStylesForStage.opacities) {
            let opacityThreshold =
              toggleStylesForStage.opacities[opacityElement];
            let el = shadowRoot.querySelector(opacityElement);

            await ContentTaskUtils.waitForCondition(
              () => {
                let opacity = parseFloat(
                  this.content.getComputedStyle(el).opacity
                );
                return opacity >= opacityThreshold;
              },
              `Toggle element ${opacityElement} should have eventually reached ` +
                `target opacity ${opacityThreshold}`,
              100,
              100
            );

            ok(true, "Toggle reached target opacity.");
          }
        }
        let { videoID, toggleClientRect, toggleStylesForStage } = args;
        let video = this.content.document.getElementById(videoID);
        let tracks = video.textTracks;
        let shadowRoot = video.openOrClosedShadowRoot;
        let closedCaptionButton = shadowRoot.querySelector(
          "#closedCaptionButton"
        );

        info("Opening text track list from cc button");
        EventUtils.synthesizeMouseAtCenter(
          closedCaptionButton,
          {},
          this.content.window
        );

        let textTrackList = shadowRoot.querySelector("#textTrackList");
        ok(textTrackList.children.length);

        info("Hovering over video to show toggle");
        await EventUtils.synthesizeMouseAtCenter(
          video,
          { type: "mousemove" },
          this.content.window
        );
        await EventUtils.synthesizeMouseAtCenter(
          video,
          { type: "mouseover" },
          this.content.window
        );

        let toggleCenterX = toggleClientRect.left + toggleClientRect.width / 2;
        let toggleCenterY = toggleClientRect.top + toggleClientRect.height / 2;

        EventUtils.synthesizeMouseAtCenter(
          closedCaptionButton,
          { type: "mouseover" },
          this.content.window
        );

        // We want to wait for the toggle to reach opacity so that we can select it.
        info("Waiting for toggle to become fully visible");
        await waitForToggleOpacity(shadowRoot, toggleStylesForStage);

        let tracksArray = Array.from(tracks);
        let trackEnabledPromise = ContentTaskUtils.waitForCondition(() =>
          tracksArray.find(track => track.mode == "showing")
        );

        info("Selecting a track over the toggle");
        EventUtils.synthesizeMouseAtPoint(
          toggleCenterX,
          toggleCenterY,
          {},
          this.content.window
        );

        let isTrackEnabled = await trackEnabledPromise;
        ok(isTrackEnabled, "A track should be enabled");
      });

      try {
        info("Verifying that no Picture-in-Picture window is open.");
        // Since we expect this condition to fail, wait for only ~1 second.
        await BrowserTestUtils.waitForCondition(
          () => Services.wm.getEnumerator(WINDOW_TYPE).hasMoreElements(),
          "Found a Picture-in-Picture window",
          100,
          10
        );
        for (let win of Services.wm.getEnumerator(WINDOW_TYPE)) {
          if (!win.closed) {
            ok(false, "Found a Picture-in-Picture window unexpectedly.");
            return;
          }
        }
      } catch (e) {
        ok(true, "No Picture-in-Picture window found");
      }
    }
  );
});
