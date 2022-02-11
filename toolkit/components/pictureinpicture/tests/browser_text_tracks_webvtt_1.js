/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
      await prepareVideosAndWebVTTTracks(browser, videoID, -1);

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
