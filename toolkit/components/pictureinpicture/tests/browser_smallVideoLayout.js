/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the Picture-in-Picture toggle is hidden when videos
 * are laid out with dimensions smaller than MIN_VIDEO_DIMENSION (a
 * constant that is also defined in videocontrols.js).
 */
add_task(async () => {
  // Most of the Picture-in-Picture tests run with the always-show
  // preference set to true to avoid the toggle visibility heuristics.
  // Since this test actually exercises those heuristics, we have
  // to temporarily disable that pref.
  //
  // We also reduce the minimum video length for displaying the toggle
  // to 5 seconds to avoid having to include or generate a 45 second long
  // video (which is the default minimum length).
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "media.videocontrols.picture-in-picture.video-toggle.always-show",
        false,
      ],
      ["media.videocontrols.picture-in-picture.video-toggle.min-video-secs", 5],
    ],
  });

  // This is the minimum size of the video in either width or height for
  // which we will show the toggle. See videocontrols.js.
  const MIN_VIDEO_DIMENSION = 140; // pixels

  for (let videoID of ["with-controls", "no-controls"]) {
    info(`Testing ${videoID} case.`);

    await BrowserTestUtils.withNewTab(
      {
        url: TEST_PAGE_WITH_SOUND,
        gBrowser,
      },
      async browser => {
        // Shrink the video down to less than MIN_VIDEO_DIMENSION.
        let targetWidth = MIN_VIDEO_DIMENSION - 1;
        await SpecialPowers.spawn(
          browser,
          [videoID, targetWidth],
          async (videoID, targetWidth) => {
            let video = content.document.getElementById(videoID);
            let shadowRoot = video.openOrClosedShadowRoot;
            let resizePromise = ContentTaskUtils.waitForEvent(
              shadowRoot.firstChild,
              "resizevideocontrols"
            );
            video.style.width = targetWidth + "px";
            await resizePromise;
          }
        );

        // The toggle should be hidden.
        await testToggleHelper(browser, videoID, false);

        // Now re-expand the video.
        await SpecialPowers.spawn(browser, [videoID], async videoID => {
          let video = content.document.getElementById(videoID);
          let shadowRoot = video.openOrClosedShadowRoot;
          let resizePromise = ContentTaskUtils.waitForEvent(
            shadowRoot.firstChild,
            "resizevideocontrols"
          );
          video.style.width = "";
          await resizePromise;
        });

        // The toggle should be visible.
        await testToggleHelper(browser, videoID, true);
      }
    );
  }
});

/**
 * Tests that when using the experimental toggle variations, videos
 * under 320px width are given the "small-video" attribute.
 */
add_task(async () => {
  const TOGGLE_SMALL = {
    rootID: "pictureInPictureToggleExperiment",
    stages: {
      hoverVideo: {
        opacities: {
          ".pip-wrapper": 0.8,
        },
        hidden: [],
      },

      hoverToggle: {
        opacities: {
          ".pip-wrapper": 1.0,
        },
        hidden: ["#pictureInPictureToggleButton", ".pip-expanded"],
      },
    },
  };

  const TOGGLE_LARGE = {
    rootID: "pictureInPictureToggleExperiment",
    stages: {
      hoverVideo: {
        opacities: {
          ".pip-small": 0.0,
          ".pip-wrapper": 0.8,
          ".pip-expanded": 0.0,
        },
        hidden: [
          "#pictureInPictureToggleButton",
          ".pip-explainer",
          ".pip-icon-label > .pip-icon",
        ],
      },
      hoverToggle: {
        opacities: {
          ".pip-small": 0.0,
          ".pip-wrapper": 1.0,
          ".pip-expanded": 1.0,
        },
        hidden: [
          "#pictureInPictureToggleButton",
          ".pip-explainer",
          ".pip-icon-label > .pip-icon",
        ],
      },
    },
  };

  // Most of the Picture-in-Picture tests run with the always-show
  // preference set to true to avoid the toggle visibility heuristics.
  // Since this test actually exercises those heuristics, we have
  // to temporarily disable that pref.
  //
  // We also reduce the minimum video length for displaying the toggle
  // to 5 seconds to avoid having to include or generate a 45 second long
  // video (which is the default minimum length).
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "media.videocontrols.picture-in-picture.video-toggle.always-show",
        false,
      ],
      ["media.videocontrols.picture-in-picture.video-toggle.min-video-secs", 5],
      ["media.videocontrols.picture-in-picture.video-toggle.mode", 1],
    ],
  });

  // Videos that are thinner than MIN_VIDEO_WIDTH should have the small-video
  // attribute set on the experimental toggle.
  const MIN_VIDEO_WIDTH = 320; // pixels

  for (let videoID of ["with-controls", "no-controls"]) {
    info(`Testing ${videoID} case.`);

    await BrowserTestUtils.withNewTab(
      {
        url: TEST_PAGE_WITH_SOUND,
        gBrowser,
      },
      async browser => {
        // Shrink the video down to less than MIN_VIDEO_WIDTH.
        let targetWidth = MIN_VIDEO_WIDTH - 1;
        let isSmallVideo = await SpecialPowers.spawn(
          browser,
          [videoID, targetWidth],
          async (videoID, targetWidth) => {
            let video = content.document.getElementById(videoID);
            let shadowRoot = video.openOrClosedShadowRoot;
            let resizePromise = ContentTaskUtils.waitForEvent(
              shadowRoot.firstChild,
              "resizevideocontrols"
            );
            video.style.width = targetWidth + "px";
            await resizePromise;
            let toggle = shadowRoot.getElementById(
              "pictureInPictureToggleExperiment"
            );
            return toggle.hasAttribute("small-video");
          }
        );

        Assert.ok(isSmallVideo, "Video should have small-video attribute");

        await testToggleHelper(browser, videoID, true, undefined, TOGGLE_SMALL);

        // Now re-expand the video.
        isSmallVideo = await SpecialPowers.spawn(
          browser,
          [videoID],
          async videoID => {
            let video = content.document.getElementById(videoID);
            let shadowRoot = video.openOrClosedShadowRoot;
            let resizePromise = ContentTaskUtils.waitForEvent(
              shadowRoot.firstChild,
              "resizevideocontrols"
            );
            video.style.width = "";
            await resizePromise;
            let toggle = shadowRoot.getElementById(
              "pictureInPictureToggleExperiment"
            );
            return toggle.hasAttribute("small-video");
          }
        );

        Assert.ok(!isSmallVideo, "Video should not have small-video attribute");

        await testToggleHelper(browser, videoID, true, undefined, TOGGLE_LARGE);
      }
    );
  }
});
