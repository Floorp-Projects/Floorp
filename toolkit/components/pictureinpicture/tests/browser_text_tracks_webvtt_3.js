/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verifies the value of a cue's .line property.
 * @param {Element} browser The <xul:browser> hosting the <video>
 * @param {String} videoID The ID of the video being checked
 * @param {Integer} trackIndex The index of the track to be loaded
 * @param {Integer} cueIndex The index of the cue to be tested on
 * @param {Integer|String} expectedValue The expected line value of the cue
 */
async function verifyLineForCue(
  browser,
  videoID,
  trackIndex,
  cueIndex,
  expectedValue
) {
  await SpecialPowers.spawn(
    browser,
    [{ videoID, trackIndex, cueIndex, expectedValue }],
    async args => {
      info("Checking .line property values");
      const video = content.document.getElementById(args.videoID);
      const activeCues = video.textTracks[args.trackIndex].activeCues;
      const vttCueLine = activeCues[args.cueIndex].line;
      is(vttCueLine, args.expectedValue, "Cue line should have expected value");
    }
  );
}

/**
 * This test ensures that text tracks appear in expected order if
 * VTTCue.line property is auto.
 */
add_task(async function test_text_tracks_new_window_line_auto() {
  info("Running test: new window - line auto");
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
      let trackIndex = 2;
      await prepareVideosAndWebVTTTracks(browser, videoID, trackIndex);

      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");
      let pipBrowser = pipWin.document.getElementById("browser");

      await verifyLineForCue(browser, videoID, trackIndex, 0, "auto");
      await verifyLineForCue(browser, videoID, trackIndex, 1, "auto");

      await SpecialPowers.spawn(pipBrowser, [], async () => {
        info("Checking text track content in pip window");
        let textTracks = content.document.getElementById("texttracks");
        ok(textTracks, "TextTracks container should exist in the pip window");
        await ContentTaskUtils.waitForCondition(() => {
          return textTracks.textContent;
        }, `Text track is still not visible on the pip window. Got ${textTracks.textContent}`);

        let cueDivs = textTracks.children;

        is(cueDivs.length, 2, "There should be 2 active cues");
        // cue1 in this case refers to the first cue to be defined in the vtt file.
        // cue2 is therefore the next cue to be defined right after in the vtt file.
        ok(
          cueDivs[0].textContent.includes("cue 2"),
          `cue 2 should be above. Got: ${cueDivs[0].textContent}`
        );
        ok(
          cueDivs[1].textContent.includes("cue 1"),
          `cue 1 should be below. Got: ${cueDivs[1].textContent}`
        );
      });
    }
  );
});

/**
 * This test ensures that text tracks appear in expected order if
 * VTTCue.line property is an integer.
 */
add_task(async function test_text_tracks_new_window_line_integer() {
  info("Running test: new window - line integer");
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
      let trackIndex = 3;
      await prepareVideosAndWebVTTTracks(browser, videoID, trackIndex);

      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");
      let pipBrowser = pipWin.document.getElementById("browser");

      await verifyLineForCue(browser, videoID, trackIndex, 0, 2);
      await verifyLineForCue(browser, videoID, trackIndex, 1, 3);
      await verifyLineForCue(browser, videoID, trackIndex, 2, 1);

      await SpecialPowers.spawn(pipBrowser, [], async () => {
        info("Checking text track content in pip window");
        let textTracks = content.document.getElementById("texttracks");
        ok(textTracks, "TextTracks container should exist in the pip window");
        await ContentTaskUtils.waitForCondition(() => {
          return textTracks.textContent;
        }, `Text track is still not visible on the pip window. Got ${textTracks.textContent}`);

        let cueDivs = textTracks.children;

        is(cueDivs.length, 3, "There should be 3 active cues");

        // cue1 in this case refers to the first cue to be defined in the vtt file.
        // cue2 is therefore the next cue to be defined right after in the vtt file.
        ok(
          cueDivs[0].textContent.includes("cue 3"),
          `cue 3 should be above. Got: ${cueDivs[0].textContent}`
        );
        ok(
          cueDivs[1].textContent.includes("cue 1"),
          `cue 1 should be next. Got: ${cueDivs[1].textContent}`
        );
        ok(
          cueDivs[2].textContent.includes("cue 2"),
          `cue 2 should be below. Got: ${cueDivs[2].textContent}`
        );
      });
    }
  );
});

/**
 * This test ensures that text tracks appear in expected order if
 * VTTCue.line property is a percentage value.
 */
add_task(async function test_text_tracks_new_window_line_percent() {
  info("Running test: new window - line percent");
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
      let trackIndex = 4;
      await prepareVideosAndWebVTTTracks(browser, videoID, trackIndex);

      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");
      let pipBrowser = pipWin.document.getElementById("browser");

      await verifyLineForCue(browser, videoID, trackIndex, 0, 90);
      await verifyLineForCue(browser, videoID, trackIndex, 1, 10);
      await verifyLineForCue(browser, videoID, trackIndex, 2, 50);

      await SpecialPowers.spawn(pipBrowser, [], async () => {
        info("Checking text track content in pip window");
        let textTracks = content.document.getElementById("texttracks");
        ok(textTracks, "TextTracks container should exist in the pip window");

        await ContentTaskUtils.waitForCondition(() => {
          return textTracks.textContent;
        }, `Text track is still not visible on the pip window. Got ${textTracks.textContent}`);

        let cueDivs = textTracks.children;
        is(cueDivs.length, 3, "There should be 3 active cues");

        // cue1 in this case refers to the first cue to be defined in the vtt file.
        // cue2 is therefore the next cue to be defined right after in the vtt file.
        ok(
          cueDivs[0].textContent.includes("cue 2"),
          `cue 2 should be above. Got: ${cueDivs[0].textContent}`
        );
        ok(
          cueDivs[1].textContent.includes("cue 3"),
          `cue 3 should be next. Got: ${cueDivs[1].textContent}`
        );
        ok(
          cueDivs[2].textContent.includes("cue 1"),
          `cue 1 should be below. Got: ${cueDivs[2].textContent}`
        );
      });
    }
  );
});
