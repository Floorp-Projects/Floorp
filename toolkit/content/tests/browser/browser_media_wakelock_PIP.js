/**
 * Test the wakelock usage for video being used in the picture-in-picuture (PIP)
 * mode. When video is playing in PIP window, we would always request a video
 * wakelock, and request audio wakelock only when video is audible.
 */
add_task(async function testCheckWakelockForPIPVideo() {
  await checkWakelockWhenChangeTabVisibility({
    description: "playing a PIP video",
    lockAudio: true,
    lockVideo: true,
  });
  await checkWakelockWhenChangeTabVisibility({
    description: "playing a muted PIP video",
    additionalParams: {
      muted: true,
    },
    lockAudio: false,
    lockVideo: true,
  });
  await checkWakelockWhenChangeTabVisibility({
    description: "playing a volume=0 PIP video",
    additionalParams: {
      volume: 0.0,
    },
    lockAudio: false,
    lockVideo: true,
  });
});

/**
 * Following are helper functions and variables.
 */
const PAGE_URL =
  "https://example.com/browser/toolkit/content/tests/browser/file_video.html";
const AUDIO_WAKELOCK_NAME = "audio-playing";
const VIDEO_WAKELOCK_NAME = "video-playing";
const TEST_VIDEO_ID = "v";

// Import this in order to use `triggerPictureInPicture()`.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/pictureinpicture/tests/head.js",
  this
);

async function checkWakelockWhenChangeTabVisibility({
  description,
  additionalParams,
  lockAudio,
  lockVideo,
}) {
  const originalTab = gBrowser.selectedTab;
  info(`start a new tab for '${description}'`);
  const tab = await BrowserTestUtils.openNewForegroundTab(
    window.gBrowser,
    PAGE_URL
  );

  info(`wait for PIP video starting playing`);
  await startPIPVideo(tab, additionalParams);
  await waitForExpectedWakeLockState(AUDIO_WAKELOCK_NAME, {
    needLock: lockAudio,
    isForegroundLock: true,
  });
  await waitForExpectedWakeLockState(VIDEO_WAKELOCK_NAME, {
    needLock: lockVideo,
    isForegroundLock: true,
  });

  info(
    `switch tab to background and still own foreground locks due to visible PIP video`
  );
  await BrowserTestUtils.switchTab(window.gBrowser, originalTab);
  await waitForExpectedWakeLockState(AUDIO_WAKELOCK_NAME, {
    needLock: lockAudio,
    isForegroundLock: true,
  });
  await waitForExpectedWakeLockState(VIDEO_WAKELOCK_NAME, {
    needLock: lockVideo,
    isForegroundLock: true,
  });

  info(`pausing PIP video should release all locks`);
  await pausePIPVideo(tab);
  await waitForExpectedWakeLockState(AUDIO_WAKELOCK_NAME, {
    needLock: false,
  });
  await waitForExpectedWakeLockState(VIDEO_WAKELOCK_NAME, {
    needLock: false,
  });

  info(`resuming PIP video should request locks again`);
  await resumePIPVideo(tab);
  await waitForExpectedWakeLockState(AUDIO_WAKELOCK_NAME, {
    needLock: lockAudio,
    isForegroundLock: true,
  });
  await waitForExpectedWakeLockState(VIDEO_WAKELOCK_NAME, {
    needLock: lockVideo,
    isForegroundLock: true,
  });

  info(`switch tab to foreground again`);
  await BrowserTestUtils.switchTab(window.gBrowser, tab);
  await waitForExpectedWakeLockState(AUDIO_WAKELOCK_NAME, {
    needLock: lockAudio,
    isForegroundLock: true,
  });
  await waitForExpectedWakeLockState(VIDEO_WAKELOCK_NAME, {
    needLock: lockVideo,
    isForegroundLock: true,
  });

  info(`remove tab`);
  await BrowserTestUtils.closeWindow(tab.PIPWindow);
  BrowserTestUtils.removeTab(tab);
}

async function startPIPVideo(tab, { muted, volume } = {}) {
  tab.PIPWindow = await triggerPictureInPicture(
    tab.linkedBrowser,
    TEST_VIDEO_ID
  );
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [muted, volume, TEST_VIDEO_ID],
    async (muted, volume, Id) => {
      const video = content.document.getElementById(Id);
      if (muted) {
        video.muted = muted;
      }
      if (volume !== undefined) {
        video.volume = volume;
      }
      ok(
        await video.play().then(
          () => true,
          () => false
        ),
        `video started playing.`
      );
    }
  );
}

function pausePIPVideo(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [TEST_VIDEO_ID], Id => {
    content.document.getElementById(Id).pause();
  });
}

function resumePIPVideo(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [TEST_VIDEO_ID], async Id => {
    await content.document.getElementById(Id).play();
  });
}
