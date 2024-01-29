/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
      await prepareVideosAndWebVTTTracks(browser, videoID);

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
      await prepareVideosAndWebVTTTracks(browser, videoID);

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
      await prepareVideosAndWebVTTTracks(browser, videoID);

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
      await prepareVideosAndWebVTTTracks(browser, videoID);

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
        await ContentTaskUtils.waitForCondition(() => {
          info(`Current text content is: ${textTracks.textContent}`);
          return !textTracks.textContent;
        }, `Text track is still visible on the pip window. Got ${textTracks.textContent}`);
      });

      await waitForNextCue(browser, videoID);

      // Wait and verify second cue
      await SpecialPowers.spawn(pipBrowser, [], async () => {
        let textTracks = content.document.getElementById("texttracks");
        info("Checking second cue in pip window");
        await ContentTaskUtils.waitForCondition(() => {
          // Cue may not appear right away after cuechange event.
          // Wait until it appears before verifying text content.
          info(`Current text content is: ${textTracks.textContent}`);
          return (
            textTracks.textContent && textTracks.textContent.includes("cue 2")
          );
        }, `Text track is still not visible on the pip window. Got ${textTracks.textContent}`);
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
      await prepareVideosAndWebVTTTracks(browser, videoID);

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
      await prepareVideosAndWebVTTTracks(browser, videoID, 2);
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

/**
 * This test ensures that the showHiddenTextTracks override correctly shows
 * text tracks with a mode of "hidden".
 */
const prepareHiddenTrackTest = () =>
  new Promise((resolve, reject) => {
    BrowserTestUtils.withNewTab(
      {
        url: TEST_PAGE_WITH_WEBVTT,
        gBrowser,
      },

      async browser => {
        const videoID = "with-controls";
        await prepareVideosAndWebVTTTracks(browser, videoID, 0, "hidden");
        await SpecialPowers.spawn(browser, [{ videoID }], async args => {
          let video = content.document.getElementById(args.videoID);
          const tracks = video.textTracks;
          Assert.strictEqual(
            tracks[0].mode,
            "hidden",
            "Track 1 mode is 'hidden'"
          );
        });

        let pipWin = await triggerPictureInPicture(browser, videoID);
        ok(pipWin, "Got Picture-in-Picture window.");
        let pipBrowser = pipWin.document.getElementById("browser");
        if (!pipBrowser) {
          reject();
        }
        resolve(pipBrowser);
      }
    );
  });

add_task(async function test_hidden_text_tracks_override() {
  info("Running test - showHiddenTextTracks");

  info("hidden mode with override");
  Services.ppmm.sharedData.set(SHARED_DATA_KEY, {
    "*://example.com/*": { showHiddenTextTracks: true },
  });
  Services.ppmm.sharedData.flush();

  await prepareHiddenTrackTest().then(async pipBrowser => {
    await SpecialPowers.spawn(pipBrowser, [], async () => {
      info("Checking text track content in pip window");
      let textTracks = content.document.getElementById("texttracks");

      // Verify text track is showing in PiP window.
      ok(textTracks, "TextTracks container should exist in the pip window");
      ok(textTracks.textContent.includes("track 1"), "Track 1 should be shown");
    });
  });

  info("hidden mode without override");
  Services.ppmm.sharedData.set(SHARED_DATA_KEY, {});
  Services.ppmm.sharedData.flush();

  await prepareHiddenTrackTest().then(async pipBrowser => {
    await SpecialPowers.spawn(pipBrowser, [], async () => {
      info("Checking text track content in pip window");
      let textTracks = content.document.getElementById("texttracks");

      // Verify text track is [not] showing in PiP window.
      ok(
        !textTracks || !textTracks.textContent.length,
        "Text track should NOT appear in PiP window."
      );
    });
  });
});
