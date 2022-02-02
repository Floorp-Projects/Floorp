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
 * Plays originating video until the next cue is loaded.
 * Once the next cue is loaded, pause the video.
 * @param {Element} browser The <xul:browser> hosting the <video>
 * @param {String} videoID The ID of the video being checked
 * @param {Integer} textTrackIndex The index of the track to be loaded, or none if -1
 */
async function waitForNextCue(browser, videoID, textTrackIndex = 0) {
  if (textTrackIndex < 0) {
    ok(false, "Cannot wait for next cue with invalid track index");
  }

  await SpecialPowers.spawn(
    browser,
    [{ videoID, textTrackIndex }],
    async args => {
      let video = content.document.getElementById(args.videoID);
      info("Playing video to activate next cue");
      video.play();
      ok(!video.paused, "Video is playing");

      info("Waiting until cuechange is called");
      await ContentTaskUtils.waitForEvent(
        video.textTracks[args.textTrackIndex],
        "cuechange"
      );

      info("Pausing video to read text track");
      video.pause();
      ok(video.paused, "Video is paused");
    }
  );
}

/**
 * This test ensures that text tracks disappear from the pip window
 * when the pref is disabled.
 */
add_task(async function test_text_tracks_existing_window_pref_disabled() {
  info("Running test: existing window - pref disabled");
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

      info("Turning off pref");
      await SpecialPowers.popPrefEnv();
      await SpecialPowers.pushPrefEnv({
        set: [
          [
            "media.videocontrols.picture-in-picture.display-text-tracks.enabled",
            false,
          ],
        ],
      });

      // Verify that cue is no longer on the pip window
      info("Checking that cue is no longer on pip window");
      await SpecialPowers.spawn(pipBrowser, [], async () => {
        let textTracks = content.document.getElementById("texttracks");
        await ContentTaskUtils.waitForCondition(() => {
          return !textTracks.textContent;
        }, `Text track is still visible on the pip window. Got ${textTracks.textContent}`);
        info("Successfully removed text tracks from pip window");
      });
    }
  );
});

/**
 * This test ensures that text tracks shown on the source video
 * window appear on an existing pip window when the pref is enabled.
 */
add_task(async function test_text_tracks_existing_window_pref_enabled() {
  info("Running test: existing window - pref enabled");
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

      info("Turning on pref");
      await SpecialPowers.popPrefEnv();
      await SpecialPowers.pushPrefEnv({
        set: [
          [
            "media.videocontrols.picture-in-picture.display-text-tracks.enabled",
            true,
          ],
        ],
      });

      info("Checking that cue is on pip window");
      await SpecialPowers.spawn(pipBrowser, [], async () => {
        let textTracks = content.document.getElementById("texttracks");
        await ContentTaskUtils.waitForCondition(() => {
          return textTracks.textContent;
        }, `Text track is still not visible on the pip window. Got ${textTracks.textContent}`);
        info("Successfully displayed text tracks on pip window");
      });
    }
  );
});

/**
 * This test ensures that text tracks update to the correct track
 * when a new track is selected.
 */
add_task(async function test_text_tracks_existing_window_new_track() {
  info("Running test: existing window - new track");
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
        ok(
          textTracks.textContent.includes("track 1"),
          "Track 1 should be loaded"
        );
      });

      // Change track in the content window
      await SpecialPowers.spawn(browser, [videoID], async videoID => {
        let video = content.document.getElementById(videoID);
        let tracks = video.textTracks;

        info("Changing to a new track");
        let track1 = tracks[0];
        track1.mode = "disabled";
        let track2 = tracks[1];
        track2.mode = "showing";
      });

      // Ensure new track is loaded
      await SpecialPowers.spawn(pipBrowser, [], async () => {
        info("Checking new text track content in pip window");
        let textTracks = content.document.getElementById("texttracks");

        await ContentTaskUtils.waitForCondition(() => {
          return textTracks.textContent;
        }, `Text track is still not visible on the pip window. Got ${textTracks.textContent}`);
        ok(
          textTracks.textContent.includes("track 2"),
          "Track 2 should be loaded"
        );
      });
    }
  );
});

/**
 * This test ensures that text tracks are correctly updated.
 */
add_task(async function test_text_tracks_existing_window_cues() {
  info("Running test: existing window - cues");
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
        let textTracks = content.document.getElementById("texttracks");
        ok(textTracks, "TextTracks container should exist in the pip window");

        // Verify that first cue appears
        info("Checking first cue on pip window");
        await ContentTaskUtils.waitForCondition(() => {
          return textTracks.textContent;
        }, `Text track is still not visible on the pip window. Got ${textTracks.textContent}`);
        ok(
          textTracks.textContent.includes("cue 1"),
          `Expected text should be displayed on the pip window. Got ${textTracks.textContent}.`
        );
      });

      // Play video to move to the next cue
      await waitForNextCue(browser, videoID);

      // Test remaining cues
      await SpecialPowers.spawn(pipBrowser, [], async () => {
        let textTracks = content.document.getElementById("texttracks");

        // Verify that empty cue makes text disappear
        info("Checking empty cue in pip window");
        ok(
          !textTracks.textContent,
          "Text track should not be visible on the pip window"
        );
      });

      await waitForNextCue(browser, videoID);

      await SpecialPowers.spawn(pipBrowser, [], async () => {
        let textTracks = content.document.getElementById("texttracks");
        // Verify that second cue appears
        info("Checking second cue in pip window");
        ok(
          textTracks.textContent.includes("cue 2"),
          "Text track second cue should be visible on the pip window"
        );
      });
    }
  );
});

/**
 * This test ensures that text tracks disappear if no track is selected.
 */
add_task(async function test_text_tracks_existing_window_no_track() {
  info("Running test: existing window - no track");
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

      // Remove track in the content window
      await SpecialPowers.spawn(browser, [videoID], async videoID => {
        let video = content.document.getElementById(videoID);
        let tracks = video.textTracks;

        info("Removing tracks");
        let track1 = tracks[0];
        track1.mode = "disabled";
        let track2 = tracks[1];
        track2.mode = "disabled";
      });

      await SpecialPowers.spawn(pipBrowser, [], async () => {
        info("Checking that text track disappears from pip window");
        let textTracks = content.document.getElementById("texttracks");

        await ContentTaskUtils.waitForCondition(() => {
          return !textTracks.textContent;
        }, `Text track is still visible on the pip window. Got ${textTracks.textContent}`);
      });
    }
  );
});

/**
 * This test ensures that text tracks appear correctly if there are multiple active cues.
 */
add_task(async function test_text_tracks_existing_window_multi_cue() {
  info("Running test: existing window - multi cue");
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
      await prepareVideosAndTracks(browser, videoID, 2);
      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");
      let pipBrowser = pipWin.document.getElementById("browser");

      await SpecialPowers.spawn(pipBrowser, [], async () => {
        info("Checking text track content in pip window");
        let textTracks = content.document.getElementById("texttracks");

        // Verify multiple active cues
        ok(textTracks, "TextTracks container should exist in the pip window");
        await ContentTaskUtils.waitForCondition(() => {
          return textTracks.textContent;
        }, `Text track is still not visible on the pip window. Got ${textTracks.textContent}`);
        is(textTracks.children.length, 2, "Text tracks should load 2 cues");
      });
    }
  );
});
