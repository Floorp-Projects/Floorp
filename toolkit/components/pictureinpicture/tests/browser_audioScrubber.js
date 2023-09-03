/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const IMPROVED_CONTROLS_ENABLED_PREF =
  "media.videocontrols.picture-in-picture.improved-video-controls.enabled";

const videoID = "with-controls";

async function getVideoVolume(browser, videoID) {
  return SpecialPowers.spawn(browser, [videoID], async videoID => {
    return content.document.getElementById(videoID).volume;
  });
}

async function getVideoMuted(browser, videoID) {
  return SpecialPowers.spawn(browser, [videoID], async videoID => {
    return content.document.getElementById(videoID).muted;
  });
}

add_task(async function test_audioScrubber() {
  await SpecialPowers.pushPrefEnv({
    set: [[IMPROVED_CONTROLS_ENABLED_PREF, true]],
  });

  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      await ensureVideosReady(browser);

      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      // Resize the PiP window so we know the audio scrubber is visible
      let resizePromise = BrowserTestUtils.waitForEvent(pipWin, "resize");
      let ratio = pipWin.innerWidth * pipWin.innerHeight;
      pipWin.resizeTo(750 * ratio, 750);
      await resizePromise;

      let audioScrubber = pipWin.document.getElementById("audio-scrubber");
      audioScrubber.focus();

      // Volume should be 1 and video should not be muted when opening this video
      let actualVolume = await getVideoVolume(browser, videoID);
      let expectedVolume = 1;

      let actualMuted = await getVideoMuted(browser, videoID);

      isfuzzy(
        actualVolume,
        expectedVolume,
        Number.EPSILON,
        `The actual volume ${actualVolume}. The expected volume ${expectedVolume}`
      );

      ok(!actualMuted, "Video is not muted");

      let volumeChangePromise = BrowserTestUtils.waitForContentEvent(
        browser,
        "volumechange",
        true
      );

      // Test that left arrow key changes volume by -0.1
      EventUtils.synthesizeKey("KEY_ArrowLeft", {}, pipWin);

      await volumeChangePromise;

      actualVolume = await getVideoVolume(browser, videoID);
      expectedVolume = 0.9;

      isfuzzy(
        actualVolume,
        expectedVolume,
        Number.EPSILON,
        `The actual volume ${actualVolume}. The expected volume ${expectedVolume}`
      );

      // Test that right arrow key changes volume by +0.1 and does not exceed 1
      EventUtils.synthesizeKey("KEY_ArrowRight", {}, pipWin);
      EventUtils.synthesizeKey("KEY_ArrowRight", {}, pipWin);
      EventUtils.synthesizeKey("KEY_ArrowRight", {}, pipWin);

      actualVolume = await getVideoVolume(browser, videoID);
      expectedVolume = 1;

      isfuzzy(
        actualVolume,
        expectedVolume,
        Number.EPSILON,
        `The actual volume ${actualVolume}. The expected volume ${expectedVolume}`
      );

      // Test that the mouse can move the audio scrubber to 0.5
      let rect = audioScrubber.getBoundingClientRect();

      volumeChangePromise = BrowserTestUtils.waitForContentEvent(
        browser,
        "volumechange",
        true
      );

      EventUtils.synthesizeMouse(
        audioScrubber,
        rect.width / 2,
        rect.height / 2,
        {},
        pipWin
      );

      await volumeChangePromise;

      actualVolume = await getVideoVolume(browser, videoID);
      expectedVolume = 0.5;

      isfuzzy(
        actualVolume,
        expectedVolume,
        0.01,
        `The actual volume ${actualVolume}. The expected volume ${expectedVolume}`
      );

      // Test muting and unmuting the video
      let muteButton = pipWin.document.getElementById("audio");
      volumeChangePromise = BrowserTestUtils.waitForContentEvent(
        browser,
        "volumechange",
        true
      );
      muteButton.click();
      await volumeChangePromise;

      ok(
        await getVideoMuted(browser, videoID),
        "The video is muted aftering clicking the mute button"
      );
      is(
        audioScrubber.max,
        "0",
        "The max of the audio scrubber is 0, so the volume slider appears that the volume is 0"
      );

      volumeChangePromise = BrowserTestUtils.waitForContentEvent(
        browser,
        "volumechange",
        true
      );
      muteButton.click();
      await volumeChangePromise;

      ok(
        !(await getVideoMuted(browser, videoID)),
        "The video is muted aftering clicking the mute button"
      );
      isfuzzy(
        actualVolume,
        expectedVolume,
        0.01,
        `The volume is still ${actualVolume}.`
      );

      volumeChangePromise = BrowserTestUtils.waitForContentEvent(
        browser,
        "volumechange",
        true
      );

      // Test that the audio scrubber can be dragged from 0.5 to 0 and the video gets muted
      EventUtils.synthesizeMouse(
        audioScrubber,
        rect.width / 2,
        rect.height / 2,
        { type: "mousedown" },
        pipWin
      );
      EventUtils.synthesizeMouse(
        audioScrubber,
        0,
        rect.height / 2,
        { type: "mousemove" },
        pipWin
      );
      EventUtils.synthesizeMouse(
        audioScrubber,
        0,
        rect.height / 2,
        { type: "mouseup" },
        pipWin
      );

      await volumeChangePromise;

      actualVolume = await getVideoVolume(browser, videoID);
      expectedVolume = 0;

      isfuzzy(
        actualVolume,
        expectedVolume,
        Number.EPSILON,
        `The actual volume ${actualVolume}. The expected volume ${expectedVolume}`
      );

      ok(
        await getVideoMuted(browser, videoID),
        "Video is now muted because slider was moved to 0"
      );

      // Test that the left arrow key does not unmute the video and the volume remains at 0
      EventUtils.synthesizeKey("KEY_ArrowLeft", {}, pipWin);
      EventUtils.synthesizeKey("KEY_ArrowLeft", {}, pipWin);

      actualVolume = await getVideoVolume(browser, videoID);

      isfuzzy(
        actualVolume,
        expectedVolume,
        Number.EPSILON,
        `The actual volume ${actualVolume}. The expected volume ${expectedVolume}`
      );

      ok(
        await getVideoMuted(browser, videoID),
        "Video is now muted because slider is still at 0"
      );

      volumeChangePromise = BrowserTestUtils.waitForContentEvent(
        browser,
        "volumechange",
        true
      );

      // Test that the right arrow key will increase the volume by +0.1 and will unmute the video
      EventUtils.synthesizeKey("KEY_ArrowRight", {}, pipWin);

      await volumeChangePromise;

      actualVolume = await getVideoVolume(browser, videoID);
      expectedVolume = 0.1;

      isfuzzy(
        actualVolume,
        expectedVolume,
        Number.EPSILON,
        `The actual volume ${actualVolume}. The expected volume ${expectedVolume}`
      );

      ok(
        !(await getVideoMuted(browser, videoID)),
        "Video is no longer muted because we moved the slider"
      );
    }
  );
});
