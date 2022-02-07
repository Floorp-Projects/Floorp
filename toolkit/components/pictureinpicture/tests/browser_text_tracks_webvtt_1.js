/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Initializes videos and text tracks for the current test case.
 * First track is the default track to be loaded onto the video.
 * Once initialization is done, play then pause the requested video.
 * so that text tracks are loaded.
 * @param {Element} browser The <xul:browser> hosting the <video>
 * @param {String} videoID The ID of the video being checked
 * @param {Integer} defaultTrackIndex The index of the track to be loaded, or none if -1
 */
async function prepareVideosAndTracks(browser, videoID, defaultTrackIndex = 0) {
  info("Preparing video and initial text tracks");
  await ensureVideosReady(browser);
  await SpecialPowers.spawn(
    browser,
    [{ videoID, defaultTrackIndex }],
    async args => {
      let video = content.document.getElementById(args.videoID);
      let tracks = video.textTracks;

      is(tracks.length, 3, "Number of tracks loaded should be 3");

      // Enable track for originating video
      if (args.defaultTrackIndex >= 0) {
        info(`Loading track ${args.defaultTrackIndex + 1}`);
        let track = tracks[args.defaultTrackIndex];
        tracks.mode = "showing";
        track.mode = "showing";
      }

      // Briefly play the video to load text tracks onto the pip window.
      info("Playing video to load text tracks");
      video.play();
      info("Pausing video");
      video.pause();
      ok(video.paused, "Video should be paused before proceeding with test");
    }
  );
}

/**
 * This test ensures that text tracks shown on the source video
 * do not appear on a newly created pip window if the pref
 * is disabled.
 */
add_task(async function test_text_tracks_new_window_pref_disabled() {
  info("Running test: new window - pref disabled");
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "media.videocontrols.picture-in-picture.display-text-tracks.enabled",
        false,
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
      await prepareVideosAndTracks(browser, videoID);

      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");
      let pipBrowser = pipWin.document.getElementById("browser");

      await SpecialPowers.spawn(pipBrowser, [], async () => {
        info("Checking text track content in pip window");
        let textTracks = content.document.getElementById("texttracks");

        ok(textTracks, "TextTracks container should exist in the pip window");
        ok(
          !textTracks.textContent,
          "Text tracks should not be visible on the pip window"
        );
      });
    }
  );
});

/**
 * This test ensures that text tracks shown on the source video
 * appear on a newly created pip window if the pref is enabled.
 */
add_task(async function test_text_tracks_new_window_pref_enabled() {
  info("Running test: new window - pref enabled");
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "media.videocontrols.picture-in-picture.display-text-tracks.enabled",
        true,
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
      await prepareVideosAndTracks(browser, videoID);

      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");
      let pipBrowser = pipWin.document.getElementById("browser");

      await SpecialPowers.spawn(pipBrowser, [], async () => {
        info("Checking text track content in pip window");
        let textTracks = content.document.getElementById("texttracks");
        ok(textTracks, "TextTracks container should exist in the pip window");
        await ContentTaskUtils.waitForCondition(() => {
          return textTracks.textContent;
        }, `Text track is still not visible on the pip window. Got ${textTracks.textContent}`);
      });
    }
  );
});

/**
 * This test ensures that text tracks do not appear on a new pip window
 * if no track is loaded and the pref is enabled.
 */
add_task(async function test_text_tracks_new_window_no_track() {
  info("Running test: new window - no track");
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "media.videocontrols.picture-in-picture.display-text-tracks.enabled",
        true,
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
      await prepareVideosAndTracks(browser, videoID, -1);

      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");
      let pipBrowser = pipWin.document.getElementById("browser");

      await SpecialPowers.spawn(pipBrowser, [], async () => {
        info("Checking text track content in pip window");
        let textTracks = content.document.getElementById("texttracks");

        ok(textTracks, "TextTracks container should exist in the pip window");
        ok(
          !textTracks.textContent,
          "Text tracks should not be visible on the pip window"
        );
      });
    }
  );
});
