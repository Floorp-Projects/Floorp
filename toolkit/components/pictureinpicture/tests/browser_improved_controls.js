/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const TEST_PAGE_LONG = TEST_ROOT + "test-video-selection.html";

const IMPROVED_CONTROLS_ENABLED_PREF =
  "media.videocontrols.picture-in-picture.improved-video-controls.enabled";

async function getVideoCurrentTime(browser, videoID) {
  return SpecialPowers.spawn(browser, [videoID], async videoID => {
    return content.document.getElementById(videoID).currentTime;
  });
}

async function getVideoDuration(browser, videoID) {
  return SpecialPowers.spawn(browser, [videoID], async videoID => {
    return content.document.getElementById(videoID).duration;
  });
}

async function timestampUpdated(timestampEl, expectedTimestamp) {
  await BrowserTestUtils.waitForMutationCondition(
    timestampEl,
    { childList: true },
    () => {
      return expectedTimestamp === timestampEl.textContent;
    }
  );
}

function checkTimeCloseEnough(actual, expected, message) {
  let equal = Math.abs(actual - expected);
  if (equal <= 0.5) {
    is(equal <= 0.5, true, message);
  } else {
    is(actual, expected, message);
  }
}

/**
 * Tests the functionality of improved Picture-in-picture
 * playback controls.
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

      await SpecialPowers.pushPrefEnv({
        set: [[IMPROVED_CONTROLS_ENABLED_PREF, true]],
      });

      // Open the video in PiP
      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      let fullscreenButton = pipWin.document.getElementById("fullscreen");
      let seekForwardButton = pipWin.document.getElementById("seekForward");
      let seekBackwardButton = pipWin.document.getElementById("seekBackward");

      // Try seek forward button
      let seekedForwardPromise = waitForVideoEvent("seeked");
      EventUtils.synthesizeMouseAtCenter(seekForwardButton, {}, pipWin);
      ok(await seekedForwardPromise, "The Forward button triggers");

      // Try seek backward button
      let seekedBackwardPromise = waitForVideoEvent("seeked");
      EventUtils.synthesizeMouseAtCenter(seekBackwardButton, {}, pipWin);
      ok(await seekedBackwardPromise, "The Backward button triggers");

      // The Fullsreen button appears when the pref is enabled and the fullscreen hidden property is set to false
      Assert.ok(!fullscreenButton.hidden, "The Fullscreen button is visible");

      // The seek Forward button appears when the pref is enabled and the seek forward button hidden property is set to false
      Assert.ok(!seekForwardButton.hidden, "The Forward button is visible");

      // The seek Backward button appears when the pref is enabled and the seek backward button hidden property is set to false
      Assert.ok(!seekBackwardButton.hidden, "The Backward button is visible");

      // CLose the PIP window
      let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      let closeButton = pipWin.document.getElementById("close");
      EventUtils.synthesizeMouseAtCenter(closeButton, {}, pipWin);
      await pipClosed;

      await SpecialPowers.pushPrefEnv({
        set: [[IMPROVED_CONTROLS_ENABLED_PREF, false]],
      });

      // Open the video in PiP
      pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      fullscreenButton = pipWin.document.getElementById("fullscreen");
      seekForwardButton = pipWin.document.getElementById("seekForward");
      seekBackwardButton = pipWin.document.getElementById("seekBackward");

      // The Fullsreen button disappears when the pref is disabled and the fullscreen hidden property is set to true
      Assert.ok(
        fullscreenButton.hidden,
        "The Fullscreen button is not visible"
      );

      // The seek Forward button disappears when the pref is disabled and the seek forward button hidden property is set to true
      Assert.ok(seekForwardButton.hidden, "The Forward button is not visible");

      // The seek Backward button disappears when the pref is disabled and the seek backward button hidden property is set to true
      Assert.ok(
        seekBackwardButton.hidden,
        "The Backward button is not visible"
      );
    }
  );
});

/**
 * Tests the functionality of Picture-in-picture
 * video scrubber
 */
add_task(async function testVideoScrubber() {
  let videoID = "long";

  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE_LONG,
      gBrowser,
    },
    async browser => {
      await ensureVideosReady(browser);

      await SpecialPowers.pushPrefEnv({
        set: [[IMPROVED_CONTROLS_ENABLED_PREF, true]],
      });

      // Open the video in PiP
      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      let scrubber = pipWin.document.getElementById("scrubber");
      scrubber.focus();

      let currentTime = await getVideoCurrentTime(browser, videoID);
      let expectedVideoTime = 0;
      const duration = await getVideoDuration(browser, videoID);
      checkTimeCloseEnough(
        currentTime,
        expectedVideoTime,
        "Video current time is 0"
      );

      let timestampEl = pipWin.document.getElementById("timestamp");
      let expectedTimestamp = "0:00 / 0:08";

      // Wait for the timestamp to update
      await timestampUpdated(timestampEl, expectedTimestamp);
      let actualTimestamp = timestampEl.textContent;
      is(actualTimestamp, expectedTimestamp, "Timestamp reads 0:00 / 0:08");

      EventUtils.synthesizeKey("KEY_ArrowRight", {}, pipWin);

      currentTime = await getVideoCurrentTime(browser, videoID);
      expectedVideoTime = 5;
      checkTimeCloseEnough(
        currentTime,
        expectedVideoTime,
        "Video current time is 5"
      );

      expectedTimestamp = "0:05 / 0:08";
      await timestampUpdated(timestampEl, expectedTimestamp);
      actualTimestamp = timestampEl.textContent;
      is(actualTimestamp, expectedTimestamp, "Timestamp reads 0:05 / 0:08");

      EventUtils.synthesizeKey("KEY_ArrowLeft", {}, pipWin);

      currentTime = await getVideoCurrentTime(browser, videoID);
      expectedVideoTime = 0;
      checkTimeCloseEnough(
        currentTime,
        expectedVideoTime,
        "Video current time is 0"
      );

      expectedTimestamp = "0:00 / 0:08";
      await timestampUpdated(timestampEl, expectedTimestamp);
      actualTimestamp = timestampEl.textContent;
      is(actualTimestamp, expectedTimestamp, "Timestamp reads 0:00 / 0:08");

      let rect = scrubber.getBoundingClientRect();

      EventUtils.synthesizeMouse(
        scrubber,
        rect.width / 2,
        rect.height / 2,
        {},
        pipWin
      );

      expectedVideoTime = duration / 2;
      currentTime = await getVideoCurrentTime(browser, videoID);
      checkTimeCloseEnough(
        currentTime,
        expectedVideoTime,
        "Video current time is 3.98..."
      );

      expectedTimestamp = "0:04 / 0:08";
      await timestampUpdated(timestampEl, expectedTimestamp);
      actualTimestamp = timestampEl.textContent;
      is(actualTimestamp, expectedTimestamp, "Timestamp reads 0:04 / 0:08");

      EventUtils.synthesizeMouse(
        scrubber,
        rect.width / 2,
        rect.height / 2,
        { type: "mousedown" },
        pipWin
      );

      EventUtils.synthesizeMouse(
        scrubber,
        rect.width,
        rect.height / 2,
        { type: "mousemove" },
        pipWin
      );

      EventUtils.synthesizeMouse(
        scrubber,
        rect.width,
        rect.height / 2,
        { type: "mouseup" },
        pipWin
      );

      expectedVideoTime = duration;
      currentTime = await getVideoCurrentTime(browser, videoID);
      checkTimeCloseEnough(
        currentTime,
        expectedVideoTime,
        "Video current time is 7.96..."
      );

      await ensureMessageAndClosePiP(browser, videoID, pipWin, false);
    }
  );
});

/**
 * Tests the behavior of the scrubber and position/duration indicator for a
 * video with an invalid/non-finite duration.
 */
add_task(async function testInvalidDuration() {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE_WITH_NAN_VIDEO_DURATION,
      gBrowser,
    },
    async browser => {
      const videoID = "nan-duration";

      // This tests skips calling ensureVideosReady, because canplaythrough
      // will never fire for the NaN duration video.

      await SpecialPowers.pushPrefEnv({
        set: [[IMPROVED_CONTROLS_ENABLED_PREF, true]],
      });

      // Open the video in PiP
      let pipWin = await triggerPictureInPicture(browser, videoID);
      ok(pipWin, "Got Picture-in-Picture window.");

      // Both the scrubber and the duration should be hidden.
      let timestampEl = pipWin.document.getElementById("timestamp");
      ok(timestampEl.hidden, "Timestamp in the PIP window should be hidden.");

      let scrubberEl = pipWin.document.getElementById("scrubber");
      ok(
        scrubberEl.hidden,
        "Scrubber control in the PIP window should be hidden"
      );
    }
  );
});
